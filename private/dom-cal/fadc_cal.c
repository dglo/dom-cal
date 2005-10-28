/*
 * fadc_cal.c
 *
 * FADC calibration.  Measures baseline, gain, and time offset from
 * ATWD.
 *
 */

#include <stdio.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "fadc_cal.h"
#include "calUtils.h"
#include "baseline_cal.h"

/*---------------------------------------------------------------------------*/
int fadc_cal(calib_data *dom_calib) {

    const int cnt = 128;
    const int fadc_cnt = 256;
    int trigger_mask;
    int i, trig, bin;
    float bias_v, peak_v;
    int peak_idx;
    
    /* Which atwd to use for FADC comparison */
    short atwd = FADC_CAL_ATWD;

    /* Channel readout buffers for each channel and bin */
    /* This test only uses one ATWD */
    short channels[3][128];

    /* FADC readout */
    short fadc[256];

    /* Average FADC waveform */
    float fadc_avg[256];

    /* Average ATWD waveform */
    float atwd_avg[128];

    /* Baseline data */
    float fadc_ref[FADC_CAL_REF_CNT];
    float fadc_baseline[FADC_CAL_REF_CNT];

    /* Peak array for ATWD and FADC */
    float peaks[FADC_CAL_TRIG_CNT];
    float fadc_peaks[FADC_CAL_TRIG_CNT];
    
    /* Charge array for ATWD and FADC */
    float charges[FADC_CAL_TRIG_CNT];
    float fadc_charges[FADC_CAL_TRIG_CNT];

    /* Gain array */
    float gains[FADC_CAL_TRIG_CNT];

#ifdef DEBUG
    printf("Performing FADC calibration...\r\n");
#endif

    /* Save DACs that we modify */
    short origBiasDAC = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);
    /* Increase sampling speed to make sure we see peak */
    short origSampDAC = halReadDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                                   DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);
    short origFADCRef = halReadDAC(DOM_HAL_DAC_FAST_ADC_REF);

    /* Set discriminator and bias level */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, FADC_CAL_PEDESTAL_DAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, FADC_CAL_DISC_DAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, FADC_CAL_SAMPLING_DAC);
    halUSleep(DAC_SET_WAIT);

    bias_v = biasDAC2V(FADC_CAL_PEDESTAL_DAC);

    /* Trigger one ATWD only */
    trigger_mask = HAL_FPGA_TEST_TRIGGER_FADC;

    /*--------------------------------------------------------------------*/
    /* Record FADC baseline as a function of DAC setting */
    
    short ref_cnt;
    for (ref_cnt = 0; ref_cnt < FADC_CAL_REF_CNT; ref_cnt++) {

        /* Change the baseline */
        fadc_ref[ref_cnt] = FADC_CAL_REF_MIN + ref_cnt * FADC_CAL_REF_STEP;
        halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, (int)fadc_ref[ref_cnt]);       
        halUSleep(DAC_SET_WAIT);

        for (i = 0; i < fadc_cnt; i++)
            fadc_avg[i] = 0.0;

        for (trig=0; trig<(int)FADC_CAL_TRIG_CNT; trig++) {
            
            /* CPU-trigger the ATWD */
            hal_FPGA_TEST_trigger_forced(trigger_mask);
            
            /* Wait for done */
            while (!hal_FPGA_TEST_readout_done(trigger_mask));
            
            /* Read out one waveform from FADC */        
            hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL,
                                  0, fadc, fadc_cnt, trigger_mask);
            
            /* Average the waveform */
            for (i = 0; i < fadc_cnt; i++) 
                fadc_avg[i] += fadc[i];
        }
        
        /* Average waveform */
        for (i = 0; i < fadc_cnt; i++) 
            fadc_avg[i] /= (float)FADC_CAL_TRIG_CNT;
        
        /* Average baseline */
        fadc_baseline[ref_cnt] = 0.0;
        for (i = 0; i < fadc_cnt; i++)
            fadc_baseline[ref_cnt] += fadc_avg[i];
        fadc_baseline[ref_cnt] /= fadc_cnt;
        
        /* TEMP FIX ME */
        printf("DAC: %d    baseline: %.2f\r\n", (int)fadc_ref[ref_cnt], fadc_baseline[ref_cnt]);
    }
    
    /* Fit relationship */
    linearFitFloat(fadc_ref, fadc_baseline, ref_cnt, &(dom_calib->fadc_baseline));
    
    /* TEMP FIX ME */
    printf("m: %g b: %g r^2: %g\r\n", dom_calib->fadc_baseline.slope,
           dom_calib->fadc_baseline.y_intercept, 
           dom_calib->fadc_baseline.r_squared);

    /*--------------------------------------------------------------------*/   
    /* Compare to ATWD channel 0 using pulser */

    /* Get sampling speed frequency in MHz */    
    float freq = getCalibFreq(atwd, *dom_calib, FADC_CAL_SAMPLING_DAC);
    float fadc_freq = FADC_CLOCK_FREQ;

    /* Set the baseline as desired */    
    short baseline_dac = (FADC_CAL_BASELINE - dom_calib->fadc_baseline.y_intercept) / 
        dom_calib->fadc_baseline.slope;

    /* Actual baseline achieved */
    float fadc_baseline_set = dom_calib->fadc_baseline.slope * baseline_dac + 
        dom_calib->fadc_baseline.y_intercept;
  
    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, baseline_dac);
    halUSleep(DAC_SET_WAIT);
    
    /* Recheck the ATWD baseline */
    float baseline[2][3];
    getBaseline(dom_calib, BASELINE_CAL_MAX_VAR, baseline);

    /* Turn on the pulser and slow down the frequency a bit */
    hal_FPGA_TEST_enable_pulser();
    hal_FPGA_TEST_set_pulser_rate(DOM_HAL_FPGA_PULSER_RATE_1_2k);

    /* OR in the ATWD to the trigger mask */
    trigger_mask |= (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* TEMP FIX ME ??? */
    /* Figure out good pulser amplitudes */
    /* Figure out how to average results */
    /* Store in the freaking struct */
    int pulser_dac;
    for (pulser_dac = 50; pulser_dac < 1000; pulser_dac += 100) {

        /* Set the pulser amplitude */
        halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, pulser_dac);

        /* Set the discriminator appropriately */
        float pulser_v = (dom_calib->pulser_calib.slope * pulser_dac) + 
            dom_calib->pulser_calib.y_intercept;

        int disc_dac = discV2DAC(0.5*pulser_v, FADC_CAL_PEDESTAL_DAC);
        halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, disc_dac);

        /* TEMP FIX ME */
        printf("Setting pulser dac to %d\r\n", pulser_dac);
        printf("Setting disc dac to %d\r\n", disc_dac);

        halUSleep(DAC_SET_WAIT);        

        /* TEMP FIX ME */
        for (i = 0; i < fadc_cnt; i++)
            fadc_avg[i] = 0.0;
        for (i = 0; i < cnt; i++)
            atwd_avg[i] = 0.0;

        /*--------------------------------------------------------------------*/
        /* Take a number of triggers */
        for (trig=0; trig<(int)FADC_CAL_TRIG_CNT; trig++) {
        
            /* Warm up the ATWD */
            prescanATWD(trigger_mask);
        
            /* Discriminator trigger the ATWD */
            hal_FPGA_TEST_trigger_disc(trigger_mask);
        
            /* Wait for done */
            while (!hal_FPGA_TEST_readout_done(trigger_mask));
        
            /* Read out one waveform for channel 0 and FADC */
            if (atwd == 0) {
                hal_FPGA_TEST_readout(channels[0], NULL, NULL, NULL, 
                                      NULL, NULL, NULL, NULL,
                                      cnt, fadc, fadc_cnt, trigger_mask);
            }
            else {
                hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                      channels[0], NULL, NULL, NULL,
                                      cnt, fadc, fadc_cnt, trigger_mask);
            }
        
            /* Find and record the peak for ATWD channel 0 */
            peak_idx = FADC_CAL_START_BIN;
            for (bin=FADC_CAL_START_BIN; bin<cnt; bin++) {

                /* Using ATWD calibration data, convert to actual V */
                if (atwd == 0) {
                    peak_v = (float)(channels[0][bin]) * dom_calib->atwd0_gain_calib[0][bin].slope
                        + dom_calib->atwd0_gain_calib[0][bin].y_intercept
                        - baseline[atwd][0];
                }
                else {
                    peak_v = (float)(channels[0][bin]) * dom_calib->atwd1_gain_calib[0][bin].slope
                        + dom_calib->atwd1_gain_calib[0][bin].y_intercept
                        - baseline[atwd][0];
                }
            
                /* Also subtract out bias voltage */
                peak_v -= bias_v;
            
                /* Note "peak" is actually a minimum */
                if (bin == FADC_CAL_START_BIN)
                    peaks[trig] = peak_v;
                else {
                    if (peak_v < peaks[trig]) {
                        peaks[trig] = peak_v;
                        peak_idx = bin;
                    }
                }
            } /* End atwd peak search */

            /* Find integral around peak */
            int int_min, int_max;
            int_min = (peak_idx - FADC_CAL_ATWD_INT_WIN_MIN >= FADC_CAL_START_BIN) ?
                peak_idx - FADC_CAL_ATWD_INT_WIN_MIN : FADC_CAL_START_BIN;
            int_max = (peak_idx + FADC_CAL_ATWD_INT_WIN_MAX <= cnt-1) ? 
                peak_idx + FADC_CAL_ATWD_INT_WIN_MAX : cnt-1;

            /* Do ATWD integral */
            charges[trig] = 0.0;       
            for (bin = int_min; bin <= int_max; bin++) {
                float v;
                if (atwd == 0) {
                    v = (float)(channels[0][bin]) * dom_calib->atwd0_gain_calib[0][bin].slope
                        + dom_calib->atwd0_gain_calib[0][bin].y_intercept
                        - baseline[atwd][0];
                }
                else {
                    v = (float)(channels[0][bin]) * dom_calib->atwd1_gain_calib[0][bin].slope
                        + dom_calib->atwd1_gain_calib[0][bin].y_intercept
                        - baseline[atwd][0];
                }            
                /* Also subtract out bias voltage */
                v -= bias_v;
                charges[trig] += v;
            }
            /* Divide out gain and frequency */
            charges[trig] /= dom_calib->amplifier_calib[0].value * freq;

            /* TEMP FIX ME */
            /* Record average waveform */
            /* Average the waveform */
            for (i = 0; i < fadc_cnt; i++) 
                fadc_avg[i] += fadc[i] - fadc_baseline_set;
            for (i = 0; i < cnt; i++) 
                atwd_avg[i] += channels[0][i];

            /* Find and record the peak for FADC */
            float fadc_val;
            peak_idx = 0;
            for (bin=0; bin<FADC_CAL_STOP_SAMPLE; bin++) {

                /* Subtract baseline */
                fadc_val = (float)fadc[bin] - fadc_baseline_set;
            
                if (bin == 0)
                    fadc_peaks[trig] = fadc_val;
                else {
                    if (fadc_val > fadc_peaks[trig]) {
                        fadc_peaks[trig] = fadc_val;
                        peak_idx = bin;                    
                    }
                }
            } /* End fadc peak search */        

            /* Do FADC integral */
            fadc_charges[trig] = 0.0;
            /* Integrate before peak */
            bin = peak_idx;
            do {
                fadc_val = fadc[bin] - fadc_baseline_set;
                fadc_charges[trig] += fadc_val;
                bin--;
            } while ((bin >= 0) && (fadc_val >= fadc_peaks[trig] * FADC_CAL_INT_FRAC));

            /* TEMP FIX ME */
            if (trig == 0) {
                printf("Stopped integration at bin %d\r\n", bin+1);
            }
            /* Integrate after peak */
            bin = peak_idx + 1;
            do {
                fadc_val = fadc[bin] - fadc_baseline_set;
                fadc_charges[trig] += fadc_val;
                bin++;
            } while ((bin < FADC_CAL_STOP_SAMPLE) && 
                     (fadc_val >= fadc_peaks[trig] * FADC_CAL_INT_FRAC));
            /* TEMP FIX ME */
            if (trig == 0) {
                printf("Stopped integration at bin %d\r\n", bin-1);
            }

            /* Divide by sampling speed */
            fadc_charges[trig] /= fadc_freq;

            /* Calculate gain for this waveform */
            gains[trig] = charges[trig] / fadc_charges[trig];

            /* TEMP FIX ME */
            if (trig == 0) {
                printf("atwd peak: %g, fadc peak %g\r\n", peaks[0], fadc_peaks[0]);
            }

        } /* End trigger loop */

        /*--------------------------------------------------------------------*/
        /* Calculate gain of FADC channel */

        float atwd_mean, atwd_var, fadc_mean, fadc_var, gain_mean, gain_var;    

        /* FIX ME I don't need all this shit */
        /* Find gain and error */
        meanVarFloat(charges, FADC_CAL_TRIG_CNT, &atwd_mean, &atwd_var);
        meanVarFloat(fadc_charges, FADC_CAL_TRIG_CNT, &fadc_mean, &fadc_var);
        meanVarFloat(gains, FADC_CAL_TRIG_CNT, &gain_mean, &gain_var);

        /* FIX ME DO VAR */

        /* TEMP FIX ME */
        printf("Mean ATWD charge: %g  var: %g\r\n", atwd_mean, atwd_var);    
        printf("Mean FADC charge: %g  var: %g\r\n", fadc_mean, fadc_var);
        printf("FADC gain: mean %g, new %g\r\n", atwd_mean / fadc_mean, gain_mean);

        /* TEMP FIX ME */
        /* Average waveform */
        for (i = 0; i < fadc_cnt; i++) {
            fadc_avg[i] /= (float)FADC_CAL_TRIG_CNT;
            fadc_avg[i] *= atwd_mean / fadc_mean;
        }
        /* Average ATWD waveform */
        for (i = 0; i < cnt; i++) {
        
            atwd_avg[i] /= (float)FADC_CAL_TRIG_CNT;

            /* Using ATWD calibration data, convert to actual V */
            if (atwd == 0) {
                atwd_avg[i] = atwd_avg[i] * dom_calib->atwd0_gain_calib[0][i].slope
                    + dom_calib->atwd0_gain_calib[0][i].y_intercept
                    - baseline[atwd][0]; 
            }
            else {
                atwd_avg[i] = atwd_avg[i] * dom_calib->atwd1_gain_calib[0][i].slope
                    + dom_calib->atwd1_gain_calib[0][i].y_intercept
                    - baseline[atwd][0];
            }
        
            /* Also subtract out bias voltage */
            atwd_avg[i] -= bias_v;
        
            atwd_avg[i] /= dom_calib->amplifier_calib[0].value;
        }

        /*--------------------------------------------------------------------*/
        /* Calculate time offset using average waveforms */

        float le_x[128], le_y[128];        
        int le_cnt;
        linear_fit le_fit;
        float atwd_le, fadc_le;
        atwd_le = fadc_le = 0.0;

        /* Get ATWD LE time */
        peak_idx = FADC_CAL_START_BIN;
        peak_v = atwd_avg[FADC_CAL_START_BIN];
        for (bin=FADC_CAL_START_BIN; bin<cnt; bin++) {
            if (atwd_avg[bin] > peak_v) {
                peak_v = atwd_avg[bin];
                peak_idx = bin;
            }
        }
        
        /* Use peak and points down to 10% for LE */
        le_cnt = 0;
        for (bin=peak_idx; bin<cnt; bin++) {
            le_x[le_cnt] = (cnt-bin-1)*1000.0/freq;
            le_y[le_cnt] = atwd_avg[bin];
            le_cnt++;
            if (atwd_avg[bin] < 0.1*peak_v) 
                break;
        }

        /* Fit LE */
        if (le_cnt >= 2) {
            linearFitFloat(le_x, le_y, le_cnt, &le_fit);
            atwd_le = -le_fit.y_intercept / le_fit.slope;                    
        }
        else {
            /* FIX ME DEAL WITH SHIT */
        }

        /* THIS IS NOT WORKING WELL -- LE IS SHIT */
        /* Get FADC LE time */
        peak_idx = 0;
        peak_v = fadc_avg[0];
        for (bin=0; bin<FADC_CAL_STOP_SAMPLE; bin++) {
            if (fadc_avg[bin] > peak_v) {
                peak_v = fadc_avg[bin];
                peak_idx = bin;
            }
        }
        
        /* Use peak and points down to 10% for LE */
        le_cnt = 0;
        for (bin=peak_idx; bin>=0; bin--) {
            le_x[le_cnt] = bin*1000.0/fadc_freq;
            le_y[le_cnt] = fadc_avg[bin];
            le_cnt++;
            if (fadc_avg[bin] < 0.1*peak_v) 
                break;
        }

        /* Fit LE */
        if (le_cnt >= 2) {
            linearFitFloat(le_x, le_y, le_cnt, &le_fit);
            fadc_le = -le_fit.y_intercept / le_fit.slope;                    
        }
        else {
            /* FIX ME DEAL WITH SHIT */
        }
        
        /* Now compute offset assuming LE are in same position */
        float delta_t = atwd_le - fadc_le;

#ifdef DEBUG
        printf("Offset between ATWD and FADC leading edges: %.1f ns\r\n", delta_t);
#endif

        /* TEMP FIX ME */
        /* Print averages */

        /* Print average waveforms */
        printf("Average ATWD ch0 waveform\r\n");
        printf("ns     mV\r\n");
        for (i=cnt-1; i >= 0; i--) {
            printf("%g %g\r\n", (cnt-i-1)*1000.0/freq, atwd_avg[i]*1000.0);
        }
        printf("Average FADC waveform\r\n");
        for (i=0; i < fadc_cnt; i++) {
            printf("%g %g\r\n", i*1000.0/fadc_freq + delta_t, fadc_avg[i]*1000.0);
        }
        

    } /* End pulser amplitude loop */

    /*--------------------------------------------------------------------*/
    /* Turn off the pulser */
    hal_FPGA_TEST_disable_pulser();

    /* Put the DACs back to original state */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);
    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, origFADCRef);

    return 0;

}
