/*
 * fadc_cal.c
 *
 * FADC calibration.  Measures baseline, gain, and time offset from
 * ATWD.
 *
 */

#include <stdio.h>
#include <stdlib.h>
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
    
    /* Charges for ATWD and FADC */
    float atwd_charge;
    float fadc_charge;

    /* Time offset between leading edges */
    float delta_t[FADC_CAL_PULSER_AMP_CNT];

    /* Gain array */
    float *gains;
    gains = (float *)malloc(sizeof(float) * FADC_CAL_TRIG_CNT * FADC_CAL_PULSER_AMP_CNT);
    float gain_mean, gain_var;    

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
        
#ifdef DEBUG
        /* TEMP FIX ME */
        printf("DAC: %d    baseline: %.2f\r\n", (int)fadc_ref[ref_cnt], fadc_baseline[ref_cnt]);
#endif
    }
    
    /* Fit relationship and store in calibration structure */
    linearFitFloat(fadc_ref, fadc_baseline, ref_cnt, &(dom_calib->fadc_baseline));
    
#ifdef DEBUG
    /* TEMP FIX ME */
    printf("FADC baseline fit  m: %g b: %g r^2: %g\r\n", dom_calib->fadc_baseline.slope,
           dom_calib->fadc_baseline.y_intercept, 
           dom_calib->fadc_baseline.r_squared);
#endif

    /*--------------------------------------------------------------------*/   
    /* Compare to ATWD channel 0 using pulser */

    /* Get sampling speed frequency in MHz */    
    float freq = getCalibFreq(atwd, *dom_calib, FADC_CAL_SAMPLING_DAC);
    float fadc_freq = FADC_CLOCK_FREQ;

    /* Set the FADC baseline as desired */    
    short baseline_dac = (FADC_CAL_BASELINE - dom_calib->fadc_baseline.y_intercept) / 
        dom_calib->fadc_baseline.slope;

    /* Actual baseline achieved */
    float fadc_baseline_set = dom_calib->fadc_baseline.slope * baseline_dac + 
        dom_calib->fadc_baseline.y_intercept;  
    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, baseline_dac);
    
    /* Turn on the pulser and slow down the frequency a bit */
    hal_FPGA_TEST_enable_pulser();
    halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, 0);
    hal_FPGA_TEST_set_pulser_rate(DOM_HAL_FPGA_PULSER_RATE_1_2k);
    halUSleep(DAC_SET_WAIT);

    /* Recheck the ATWD baseline */
    float baseline[2][3];
    getBaseline(dom_calib, BASELINE_CAL_MAX_VAR, baseline);

    /* OR in the ATWD to the trigger mask */
    trigger_mask |= (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /*--------------------------------------------------------------------*/   
    /* Loop over various pulser amplitudes and compute FADC gain */
    int pulser_dac, pulser_cnt;
    for (pulser_cnt = 0; pulser_cnt < FADC_CAL_PULSER_AMP_CNT; pulser_cnt++) {

        /* Set the pulser amplitude */
        pulser_dac = FADC_CAL_PULSER_AMP_MIN + pulser_cnt*FADC_CAL_PULSER_AMP_STEP;
        halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, pulser_dac);

#ifdef DEBUG
        /* TEMP FIX ME */
        printf("Setting pulser dac to %d\r\n", pulser_dac);
#endif
        halUSleep(DAC_SET_WAIT);        

        /* Initialize average waveform arrays */
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
            atwd_charge = 0.0;       
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
                atwd_charge += v;
            }
            /* Divide out gain and frequency */
            atwd_charge /= dom_calib->amplifier_calib[0].value * freq;

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
            fadc_charge = 0.0;
            /* Integrate before peak */
            bin = peak_idx;
            do {
                fadc_val = fadc[bin] - fadc_baseline_set;
                fadc_charge += fadc_val;
                bin--;
            } while ((bin >= 0) && (fadc_val >= fadc_peaks[trig] * FADC_CAL_INT_FRAC));

            /* Integrate after peak */
            bin = peak_idx + 1;
            do {
                fadc_val = fadc[bin] - fadc_baseline_set;
                fadc_charge += fadc_val;
                bin++;
            } while ((bin < FADC_CAL_STOP_SAMPLE) && 
                     (fadc_val >= fadc_peaks[trig] * FADC_CAL_INT_FRAC));

            /* Divide by sampling speed */
            fadc_charge /= fadc_freq;

            /* Calculate gain for this waveform */
            gains[pulser_cnt*FADC_CAL_TRIG_CNT + trig] = atwd_charge / fadc_charge;

        } /* End trigger loop */

        /*--------------------------------------------------------------------*/
        /* Calculate gain of FADC channel for this pulser setting */

        /* Find gain and error */
        meanVarFloat(gains+(pulser_cnt*FADC_CAL_TRIG_CNT), FADC_CAL_TRIG_CNT, 
                     &gain_mean, &gain_var);

#ifdef DEBUG
        /* TEMP FIX ME */
        printf("FADC gain: %g\r\n", gain_mean);
#endif

        /* Average waveform */
        for (i = 0; i < fadc_cnt; i++) {
            fadc_avg[i] /= (float)FADC_CAL_TRIG_CNT;
            fadc_avg[i] *= gain_mean;
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
#ifdef DEBUG
            printf("WARNING: too few points to fit ATWD leading edge!\n");
#endif
            /* Approximate leading edge */
            atwd_le = le_x[le_cnt-1];
        }

        /* This could have a significant systematic offset */
        /* The FADC LE is extremely poor */
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
#ifdef DEBUG
            printf("WARNING: too few points to fit FADC leading edge!\n");
#endif
            /* Approximate leading edge */
            fadc_le = le_x[le_cnt-1];
        }
        
        /* Now compute offset assuming LE are in same position */
        delta_t[pulser_cnt] = atwd_le - fadc_le;

#ifdef DEBUG
        /* TEMP FIX ME */
        printf("Offset between ATWD and FADC leading edges: %.1f ns\r\n", delta_t[pulser_cnt]);
#endif

    } /* End pulser amplitude loop */

    /* Calculate average gain over all pulser settings */
    meanVarFloat(gains, FADC_CAL_TRIG_CNT*FADC_CAL_PULSER_AMP_CNT, 
                 &gain_mean, &gain_var);
    dom_calib->fadc_gain.value = gain_mean;
    dom_calib->fadc_gain.error = sqrt(gain_var / (FADC_CAL_TRIG_CNT*FADC_CAL_PULSER_AMP_CNT));
    
    /* Calculate delta_t average over all pulser settings */
    float delta_t_mean, delta_t_var;
    meanVarFloat(delta_t, FADC_CAL_PULSER_AMP_CNT, &delta_t_mean, &delta_t_var);
    dom_calib->fadc_delta_t.value = delta_t_mean;
    dom_calib->fadc_delta_t.error = sqrt(delta_t_var / FADC_CAL_PULSER_AMP_CNT);

    /*--------------------------------------------------------------------*/
    /* Turn off the pulser */
    hal_FPGA_TEST_disable_pulser();

    /* Put the DACs back to original state */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);
    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, origFADCRef);

    /* Free mallocs */
    free(gains);

    return 0;

}
