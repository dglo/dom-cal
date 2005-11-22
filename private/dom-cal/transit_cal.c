/*
 * transit_cal
 *
 * Transit time calibration -- measure mainboard LED light pulse and
 * current pulse with LED-triggered waveforms, and compute offset.  
 * Includes PMT transit time and delay board.
 *
 */

#include <stdio.h>
#include <stdlib.h>
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
    int hv_tt_valid[TRANSIT_CAL_HV_CNT];
    float le_x[128], le_y[128];
    int le_cnt;
    linear_fit le_fit;

    /* Which atwd to use */
    short atwd = TRANSIT_CAL_ATWD;
    
    /* Which channel to record light output */
    ch = TRANSIT_CAL_CH;

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
    float transit_data[TRANSIT_CAL_HV_CNT];

#ifdef DEBUG
    printf("Performing PMT transit time calibration (using ATWD%d)...\r\n", atwd);
#endif

    /* Save DACs that we modify */
    short origBiasDAC = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origSampDAC = halReadDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                                   DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);

    /* Set discriminator and bias level */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, TRANSIT_CAL_PEDESTAL_DAC);   
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, TRANSIT_CAL_SAMPLING_DAC);
    
    /* Set the launch delay */
    hal_FPGA_TEST_set_atwd_LED_delay(TRANSIT_CAL_LAUNCH_DELAY);

    /* Get bias voltage */
    bias_v = biasDAC2V(TRANSIT_CAL_PEDESTAL_DAC);
   
    /* Get sampling speed frequency in MHz */    
    float freq = getCalibFreq(atwd, *dom_calib, TRANSIT_CAL_SAMPLING_DAC);

    /* Select mainboard LED */
    halSelectAnalogMuxInput(DOM_HAL_MUX_PMT_LED_CURRENT);
    
    /* Need to use LED triggers, because the 20MHz clock couples into the signal */
    /* Using LED triggers without LED power, we can largely subtract this out! */
    halDisableLEDPS();

    /* Start "flashing" */
    hal_FPGA_TEST_enable_LED();

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

    /* Ensure HV base exists before performing calibration */
    if (!checkHVBase()) {
        dom_calib->transit_calib_valid = 0;
        dom_calib->transit_calib.slope = 0.0; 
        dom_calib->transit_calib.y_intercept = 0.0;
        dom_calib->transit_calib.r_squared = 0.0;
        return TRANSIT_CAL_NO_HV_BASE;
    }
    
    /*---------------------------------------------------------------------------*/    
    /* Measure pedestal of channel 3 */

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
    for (bin = 0; bin<cnt; bin++)
        pedestal[bin] /= TRANSIT_CAL_TRIG_CNT;

    /*----------------------------------------------------------------------------*/
    /* HV LOOP */
    /*----------------------------------------------------------------------------*/

    /* Loop over HV settings */
    int hv_idx, peak_idx;
    int peak_atwd;
    float le_atwd_idx, le_current_idx;

    for (hv_idx = 0; hv_idx < TRANSIT_CAL_HV_CNT; hv_idx++) {

        float peak_avg = 0.0;
        float current_peak_avg = 0.0;
        float peak_atwd_avg = 0.0;       
        int fail = 0;

        /* Make sure the LED is set to minimum brightness and cycle power */
        halDisableLEDPS();
        halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, TRANSIT_CAL_LED_AMP_START);
        halUSleep(DAC_SET_WAIT);
        halEnableLEDPS();

        /* Initialize this calibration point as invalid */
        hv_tt_valid[hv_idx] = 0;

        /* Set high voltage and give it time to stabilize */
        hv = (hv_idx * TRANSIT_CAL_HV_INC) + TRANSIT_CAL_HV_LOW;      

#ifdef DEBUG
        printf("Setting HV to %d V\r\n", hv);
#endif
        halWriteActiveBaseDAC(hv * 2);
        halUSleep(5000000);

        /* Get baseline */
        float baseline[2][3];
        getBaseline(dom_calib, BASELINE_CAL_MAX_VAR, baseline);

        /* Find a good brightness setting for this HV */
        int brightness;
        for (brightness = TRANSIT_CAL_LED_AMP_START; 
             brightness > TRANSIT_CAL_LED_AMP_STOP; brightness -= TRANSIT_CAL_LED_AMP_STEP) {

            /* Reset average! */
            peak_atwd_avg = 0.0;

            /* Change to new brightness */
            /* Just too slow to wait a full second here */
            halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, brightness);
            halUSleep(10000);

            /* Take some waveforms */
            for (trig=0; trig<(int)TRANSIT_CAL_AMP_TRIG; trig++) {
            
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
                
                /* Find the peak in the ATWD waveform */            
                peak_idx = 0;
                peak_atwd = channels[ch][0];                
                for (bin=0; bin<cnt; bin++) {
                    if (channels[ch][bin] > peak_atwd) {
                        peak_idx = bin;
                        peak_atwd = channels[ch][bin];
                    }
                }
                /* Calculate peak average */
                peak_atwd_avg += peak_atwd;                                

            } /* End brightness check peak loop */

            /* Check if average peak is within acceptable range */
            peak_atwd_avg /= TRANSIT_CAL_AMP_TRIG;

            if ((peak_atwd_avg >= TRANSIT_CAL_ATWD_AMP_LOW) &&
                (peak_atwd_avg <= TRANSIT_CAL_ATWD_AMP_HIGH)) break;
        }
        
        /* Did the brightness search fail? */
        /* If so, skip to the next HV setting */
        if (brightness < TRANSIT_CAL_LED_AMP_STOP) {
#ifdef DEBUG
            printf("Couldn't find an acceptable LED brightness during scan!  Trying next voltage.\r\n");
#endif
            continue;
        }
        else {
#ifdef DEBUG
            printf("Using brightness %d for ATWD peak of %.1f\r\n", brightness, peak_atwd_avg);
#endif
        }

        /*----------------------------------------------------------------------------*/
        /* Now do the real measurement */
        /*----------------------------------------------------------------------------*/

        /* reset averages */
        peak_avg = 0.0;
        current_peak_avg = 0.0;

        int no_peaks = 0;
        int bad_le = 0;

        /* Minimum acceptable light output */
        float transit_cal_min_peak_v;
        if (atwd == 0) {
            transit_cal_min_peak_v = (float)TRANSIT_CAL_ATWD_AMP_LOW * 
                dom_calib->atwd0_gain_calib[ch][bin].slope + dom_calib->atwd0_gain_calib[ch][bin].y_intercept;
        }
        else {
            transit_cal_min_peak_v = (float)TRANSIT_CAL_ATWD_AMP_LOW * 
                dom_calib->atwd1_gain_calib[ch][bin].slope + dom_calib->atwd1_gain_calib[ch][bin].y_intercept;
        }
        transit_cal_min_peak_v -= baseline[atwd][ch];
        transit_cal_min_peak_v -= bias_v;
        transit_cal_min_peak_v = 0.7*fabs(transit_cal_min_peak_v);
        
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
            
            /* Find the peak in the ATWD waveform */            
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
                    bin_v -= baseline[1][ch];
                    bin_v -= bias_v;
                }
                
                if (bin_v < peak_v) {
                    peak_idx = bin;
                    peak_v = bin_v;
                }
                
            }
            
            /* Make sure there is a peak! */
            if (fabs(peak_v) < transit_cal_min_peak_v) {
                no_peaks++;
                
                /* Too many triggers without a decent peak? */
                if (no_peaks > TRANSIT_CAL_MAX_NO_PEAKS) {
                    /* Increase brightness! */
                    brightness -= TRANSIT_CAL_LED_AMP_STEP;
                    fail = (brightness < TRANSIT_CAL_LED_AMP_STOP);
                    
                    if (fail) {
#ifdef DEBUG
                        printf("Couldn't find acceptable brightness during data-taking.\r\n");
                        printf("Trying next HV setting.\r\n");
#endif                        
                        break; /* Out of trigger loop! */
                    }
                    else {
                        /* Start trigger loop over again with higher brightness */
                        no_peaks = 0;
                        trig = 0;
                        halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, brightness);
                        halUSleep(DAC_SET_WAIT);
#ifdef DEBUG
                        printf("Too few light pulses -- increasing brightness to %d\r\n", brightness);
#endif
                    }
                }
                else
                    trig--;
                continue; /* trigger loop */
            }
            else {
                /* Calculate peak average, for kicks */
                peak_avg += peak_v;                            
            }
            
            /* Fit leading edge of light waveform */
            /* LE is point where this line intersects the baseline */
            le_cnt = 0;
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
                    bin_v -= baseline[1][ch];
                    bin_v -= bias_v;
                }
                
                /* Use points from 10% to 90% level for the fit */
                /* Recall peak is negative! */
                if ((bin_v <= 0.1*peak_v) && (bin_v >= 0.9*peak_v)) {
                    le_x[le_cnt] = bin;
                    le_y[le_cnt] = bin_v; 
                    le_cnt++;
                }
                else if (bin_v > 0.1*peak_v)
                    break;
            }
            if (le_cnt >= 2) {
                linearFitFloat(le_x, le_y, le_cnt, &le_fit);
                le_atwd_idx = -le_fit.y_intercept / le_fit.slope;                    
            }
            else {
                bad_le++;
                if (bad_le > TRANSIT_CAL_MAX_BAD_LE) {
#ifdef DEBUG
                    printf("Too many bad leading edges.  Trying next HV setting.\r\n");
#endif
                    fail = 1;
                    break; /* out of trigger loop */
                }
                else
                    trig--;
                continue;                    
            }
                        
            /* Find the peak and leading edge in the current waveform */
            /* Note polarity of pedestal subtraction to keep peak a minimum like in ATWD */
            /* Also -- baseline is off, use first few samples as an average */
            float ch3_baseline = (float)((pedestal[0] - channels[3][0]) +
                                         (pedestal[1] - channels[3][1]) +
                                         (pedestal[2] - channels[3][2])) / 3.0;
            peak_v = pedestal[0] - channels[3][0] - ch3_baseline;
            peak_idx = 0;
            for (bin=0; bin<cnt; bin++) {
                
                bin_v = pedestal[bin] - channels[3][bin] - ch3_baseline;
                if (bin_v < peak_v) {
                    peak_idx = bin;
                    peak_v = bin_v;
                }
            }           
            
            /* Calculate peak average, for kicks */
            current_peak_avg += peak_v;
            
            /* Current leading edge is too sharp to fit -- besides, we really want point */
            /* when LED turns on, which is certainly not at exactly the LE. */
            /* Also, 50% point is independent of current pulse amplitude to 0.2 ns or so */
            float last_bin_v = peak_v;
            le_current_idx = 0.0;
            for (bin=peak_idx; bin<cnt; bin++) {
                bin_v = pedestal[bin] - channels[3][bin] - ch3_baseline;
                
                if (bin_v > TRANSIT_CAL_EDGE_FRACT*peak_v) {
                    /* Interpolate */
                    le_current_idx = (bin-1) + 
                        (TRANSIT_CAL_EDGE_FRACT*peak_v - last_bin_v)/(bin_v - last_bin_v);
                    break;
                }
                last_bin_v = bin_v;
            }
            
            /* Save transit time in ns = samples * 1000 / freq in MHz */
            transits[trig] = (le_current_idx - le_atwd_idx) * 1.0E3 / freq;
            
            
        } /* End trigger loop */
        
        if (!fail) {
            /* Print average peak amplitude */
            peak_avg /= TRANSIT_CAL_TRIG_CNT;
            current_peak_avg /= TRANSIT_CAL_TRIG_CNT;
            
#ifdef DEBUG
            printf("V %d Avg signal peak %.2f  Avg current peak %.2f\r\n", hv, peak_avg, current_peak_avg);
            printf("Triggers with too little light: %d\r\n", no_peaks);
#endif
            
            /* Find mean and error */
            float var;
            meanVarFloat(transits, TRANSIT_CAL_TRIG_CNT, 
                         &(transit_data[hv_idx]), &var);
            float sigma = sqrt(var);
                        
#ifdef DEBUG
            printf("tt: %.2f sigma: %.3f\r\n", transit_data[hv_idx], sigma);
#endif
            
            /* Check sigma */
            if (sigma <= TRANSIT_CAL_MAX_SIGMA) 
                hv_tt_valid[hv_idx] = 1;
            else {
#ifdef DEBUG
                printf("Sigma too high; invalidating this HV point.\r\n");
#endif
            }
        } /* End if no fail */
       
    } /* End HV loop */

    /*---------------------------------------------------------------------------*/    
    /* Attempt 1 / sqrt(V) fit */
    /*---------------------------------------------------------------------------*/
    
    float x[TRANSIT_CAL_HV_CNT], y[TRANSIT_CAL_HV_CNT];    
    int vld_cnt = 0;
    for (hv_idx = 0; hv_idx < TRANSIT_CAL_HV_CNT; hv_idx++) {
        if (hv_tt_valid[hv_idx]) {
            int hv = (hv_idx * TRANSIT_CAL_HV_INC) + TRANSIT_CAL_HV_LOW;
            x[vld_cnt] = 1 / sqrt(hv);
            y[vld_cnt] = transit_data[hv_idx];
#ifdef DEBUG
            printf("%d %g %g\r\n", hv, x[vld_cnt], y[vld_cnt]);
#endif
            vld_cnt++;
        }
    }

    if (vld_cnt >= TRANSIT_CAL_MIN_VLD_PTS) {
        linearFitFloat(x, y, vld_cnt, &dom_calib->transit_calib);
        dom_calib->transit_calib_valid = 1;
        dom_calib->transit_calib_points = vld_cnt;
    }
    else {
#ifdef DEBUG
        printf("ERROR: too few valid points for transit time fit.  Aborting.\r\n");
        dom_calib->transit_calib_valid = 0;
        dom_calib->transit_calib.slope = 0.0; 
        dom_calib->transit_calib.y_intercept = 0.0;
        dom_calib->transit_calib.r_squared = 0.0;
        return TRANSIT_CAL_PTS_ERR;
#endif
    }

#ifdef DEBUG
    printf("Fit: m %g b %g r2 %g\r\n", dom_calib->transit_calib.slope,
            dom_calib->transit_calib.y_intercept, dom_calib->transit_calib.r_squared);
#endif

    /*---------------------------------------------------------------------------*/
    /* Turn of MB LED */
    halDisableLEDPS();
    hal_FPGA_TEST_disable_LED();
    
    /* Put the DACs back to original state */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);
    
    /* Disable the analog mux */
    halDisableAnalogMux();
    
    /* Won't turn off the HV for now...*/
    
    return 0;
    
}
