/*
 * transit_cal
 *
 * Transit time calibration -- measure mainboard LED light pulse and
 * current pulse with LED-triggered waveforms, and compute offset.  
 * Includes PMT transit time and delay board.
 *
 */

#include <stdio.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "calUtils.h"
#include "baseline_cal.h"
#include "transit_cal.h"

/*---------------------------------------------------------------------------*/
int transit_cal(calib_data *dom_calib) {

    const int cnt = 128;
    int trigger_mask;
    int ch, bin, trig;
    float bias_v, peak_v, bin_v;
    int hv;
        
    /* Which atwd to use */
    short atwd = TRANSIT_CAL_ATWD;
    
    /* Trigger one ATWD only */
    trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Channel readout buffers for each channel and bin */
    /* This test only uses one ATWD */
    short channels[4][128];

    /* Pedestal pattern in channel 3 */
    float pedestal[128];

    /* Leading-edge differences btw current and light */
    float transits[TRANSIT_CAL_TRIG_CNT];

    /* Results -- mean and error at each point */
    static transit_times transit_array[TRANSIT_CAL_HV_CNT];

#ifdef DEBUG
    printf("Performing PMT transit time calibration (using ATWD%d)...\r\n", atwd);
#endif

    /* Save DACs that we modify */
    short origBiasDAC = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origSampDAC = halReadDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                                   DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);
    short origLedDAC  = halReadDAC(DOM_HAL_DAC_LED_BRIGHTNESS);

    /* Set discriminator and bias level */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, TRANSIT_CAL_PEDESTAL_DAC);   
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, TRANSIT_CAL_SAMPLING_DAC);
    
    /* Set the launch delay */
    hal_FPGA_TEST_set_atwd_LED_delay(TRANSIT_CAL_LAUNCH_DELAY);

    /* Enable the analog mux and select the mainboard LED current */
    halSelectAnalogMuxInput(DOM_HAL_MUX_PMT_LED_CURRENT);

    /* Get bias voltage */
    bias_v = biasDAC2V(TRANSIT_CAL_PEDESTAL_DAC);
   
    /* Get sampling speed frequency in MHz */    
    float freq = getCalibFreq(atwd, *dom_calib, TRANSIT_CAL_SAMPLING_DAC);

    /* Give user a final warning */
#ifdef DEBUG
    printf(" *** WARNING: enabling HV in 5 seconds! ***\r\n");
#endif
    halUSleep(5000000);

    /* Turn on high voltage base */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halEnablePMT_HV();
#else
    halPowerUpBase();
    halEnableBaseHV();
#endif

    /*---------------------------------------------------------------------------*/    
    /* Turn on the LED power supply before measuring pedestal */                

    /* Need to use LED triggers, because the 20MHz clock couples into the signal */
    /* Using LED triggers without LED power, we can largely subtract this out! */

    halDisableLEDPS();

    /* Start "flashing" */
    hal_FPGA_TEST_enable_LED();

    /* Get pedestal for channel 3 (uncalibrated!) */
    for (bin=0; bin<cnt; bin++)
        pedestal[bin] = 0.0;

    for (trig=0; trig<(int)TRANSIT_CAL_TRIG_CNT; trig++) {
            
        /* Warm up the ATWD */
        prescanATWD(trigger_mask);
            
        /* LED-trigger the ATWD */
        hal_FPGA_TEST_trigger_LED(trigger_mask);
            
            /* Wait for done */
        while (!hal_FPGA_TEST_readout_done(trigger_mask));
        
        /* Read out one waveform for channel 3 */        
        if (atwd == 0) {
            hal_FPGA_TEST_readout(NULL, NULL, NULL, channels[3],
                                  NULL, NULL, NULL, NULL,
                                  cnt, NULL, 0, trigger_mask);
        }
        else {
            hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, channels[3],
                                  cnt, NULL, 0, trigger_mask);
        }

        for(bin = 0; bin<cnt; bin++)
            pedestal[bin] += (float)channels[3][bin];
        
    }

    /* Get the average */
    /* printf("Pedestal pattern:\r\n"); */
    for (bin = 0; bin<cnt; bin++) {
        pedestal[bin] /= TRANSIT_CAL_TRIG_CNT;
        /* printf("%.2f\r\n", pedestal[bin]); */
    }

    /* Turn on the LED power */
    halEnableLEDPS();

    /* Set the LED brightness */
    halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, TRANSIT_CAL_LED_AMPLITUDE);

    /* Loop over HV settings */
    int hv_idx, peak_idx;
    float le_atwd_idx, le_current_idx;

    for (hv_idx = 0; hv_idx < TRANSIT_CAL_HV_CNT; hv_idx++) {

        float peak_avg = 0.0;

        /* Set high voltage and give it time to stabilize */
        hv = (hv_idx * TRANSIT_CAL_HV_INC) + TRANSIT_CAL_HV_LOW;      
        
#ifdef DEBUG
        printf(" Setting HV to %d V\r\n", hv);
#endif
        halWriteActiveBaseDAC(hv * 2);
        halUSleep(5000000);

        /* Get baseline */
        float baseline[2][3];
        getBaseline(dom_calib, BASELINE_CAL_MAX_VAR, baseline);

        /* Take a number of waveforms */
        for (trig=0; trig<(int)TRANSIT_CAL_TRIG_CNT; trig++) {
            
            /* Warm up the ATWD */
            prescanATWD(trigger_mask);
            
            /* LED-trigger the ATWD */
            hal_FPGA_TEST_trigger_LED(trigger_mask);
            
            /* Wait for done */
            while (!hal_FPGA_TEST_readout_done(trigger_mask));
            
            /* Read out one waveform for all channels */        
            if (atwd == 0) {
                hal_FPGA_TEST_readout(channels[0], channels[1], channels[2], channels[3],
                                      NULL, NULL, NULL, NULL,
                                      cnt, NULL, 0, trigger_mask);
            }
            else {
                hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                      channels[0], channels[1], channels[2], channels[3],
                                      cnt, NULL, 0, trigger_mask);
            }

            /* FIX ME -- CHECK AMPLITUDE? */
            ch = TRANSIT_CAL_CH;

            /* Find the peak in the ATWD waveform */                
            /* FIX ME DEBUG */
            /* if (trig == 0) 
               printf("Channel %d\r\n", ch); */
            
            peak_idx = 0;
            if (atwd == 0) {
                peak_v = (float)channels[ch][0] * dom_calib->atwd0_gain_calib[ch][0].slope
                    + dom_calib->atwd0_gain_calib[ch][0].y_intercept;
            }
            else {
                peak_v = (float)channels[ch][0] * dom_calib->atwd1_gain_calib[ch][0].slope
                    + dom_calib->atwd1_gain_calib[ch][0].y_intercept;
            }
            peak_v -= baseline[atwd][ch];
            peak_v -= bias_v;

            for (bin=0; bin<cnt; bin++) {
                
                /* Use calibration to convert to V */
                /* Don't need to subtract out bias or correct for amplification to find */
                /* peak location -- but without correction, it is really a minimum */
                if (atwd == 0) {
                    bin_v = (float)channels[ch][bin] * dom_calib->atwd0_gain_calib[ch][bin].slope
                        + dom_calib->atwd0_gain_calib[ch][bin].y_intercept;
                    bin_v -= baseline[0][ch];
                    bin_v -= bias_v;
                }
                else {
                    bin_v = (float)channels[ch][bin] * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept;
                    bin_v -= baseline[0][ch];
                    bin_v -= bias_v;
                }
                
                if (bin_v < peak_v) {
                    peak_idx = bin;
                    peak_v = bin_v;
                }
                
                /* FIX ME DEBUG */
                if (trig==0)
                    printf("%d %g\r\n", bin, bin_v);
            }
            
            /* Calculate peak average, for kicks */
            peak_avg += peak_v;

            /* Now find leading edge in ATWD waveform */
            float last_bin_v = peak_v;
            le_atwd_idx = 0.0;    
            for (bin=peak_idx; bin<cnt; bin++) {
                if (atwd == 0) {
                    bin_v = (float)channels[ch][bin] * dom_calib->atwd0_gain_calib[ch][bin].slope
                        + dom_calib->atwd0_gain_calib[ch][bin].y_intercept;
                    bin_v -= baseline[0][ch];
                    bin_v -= bias_v;
                }
                else {
                    bin_v = (float)channels[ch][bin] * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept;
                    bin_v -= baseline[0][ch];
                    bin_v -= bias_v;
                }
                
                if (bin_v > 0.5*peak_v) {                    
                    /* Interpolate */
                    le_atwd_idx = (bin-1) + (0.5*peak_v - last_bin_v)/(bin_v - last_bin_v);
                    break;
                }
                last_bin_v = bin_v;
            }
           
            /* FIX ME DEBUG */
            /* printf("atwd LE: %.1f\r\n", le_atwd_idx); */

            /* Find the peak and leading edge in the current waveform */
            /* Note polarity of pedestal subtraction to keep peak a minimum like in ATWD */
            peak_v = pedestal[0] - channels[3][0];
            peak_idx = 0;
            for (bin=0; bin<cnt; bin++) {
                
                bin_v = pedestal[bin] - channels[3][bin];
                if (bin_v < peak_v) {
                    peak_idx = bin;
                    peak_v = bin_v;
                }

                /* FIX ME DEBUG */
                /* if (trig==0)
                   printf("%d %g\r\n", bin, bin_v); */

            }           

            /* Now find leading edge in current waveform */
            last_bin_v = peak_v;
            le_current_idx = 0.0;
            for (bin=peak_idx; bin<cnt; bin++) {
                bin_v = pedestal[bin] - channels[3][bin];

                if (bin_v > 0.5*peak_v) {
                    /* Interpolate */
                    le_current_idx = (bin-1) + (0.5*peak_v - last_bin_v)/(bin_v - last_bin_v);
                    break;
                }
                last_bin_v = bin_v;
            }
            
            /* FIX ME DEBUG */
            /* printf("current LE: %.1f\r\n", le_current_idx); */
            
            /* FIX ME CHECK FOR FREAKING ERRORS */
        
            /* Save transit time in ns = samples * 1000 / freq in MHz */
            transits[trig] = (le_current_idx - le_atwd_idx) * 1.0E3 / freq;

        } /* End trigger loop */

        /* Print average peak amplitude */
        peak_avg /= TRANSIT_CAL_TRIG_CNT;
        printf("V %d Avg peak %.2f\r\n", hv, peak_avg);

        /* Find mean and error */
        float var;
        meanVarFloat(transits, TRANSIT_CAL_TRIG_CNT, 
                     &(transit_array[hv_idx].transit_data.value), &var);

        printf("Sqrt(var): %.3f\r\n", sqrt(var));

        transit_array[hv_idx].transit_data.error = sqrt(var) / sqrt(TRANSIT_CAL_TRIG_CNT);        
        transit_array[hv_idx].voltage = hv;

    } /* End HV loop */

    /*---------------------------------------------------------------------------*/
    /* Save results */

#ifdef DEBUG
    printf("HV value error\r\n");
    for (hv_idx = 0; hv_idx < TRANSIT_CAL_HV_CNT; hv_idx++) {        
        printf("%d %g %g\r\n", hv_idx,  transit_array[hv_idx].transit_data.value,
               transit_array[hv_idx].transit_data.error);
    }    
#endif
    dom_calib->transit_calib = transit_array;

    /*---------------------------------------------------------------------------*/
    /* Turn off LED and LED power supply*/

    halDisableLEDPS();
    hal_FPGA_TEST_disable_LED();
    
    /* Put the DACs back to original state */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);
    halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, origLedDAC);
    
    /* Disable the analog mux */
    halDisableAnalogMux();
    
    /* Won't turn off the HV for now...*/
    
    /* FIX ME: return real error code */
    return 0;
    
}