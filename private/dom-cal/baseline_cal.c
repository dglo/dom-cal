/*
 * baseline_cal
 *
 * Calibrate out any remaining baseline after ATWD calibration.
 * Can iterate over a number of HV settings.
 *
 */

#include <stdio.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "baseline_cal.h"
#include "hv_gain_cal.h"
#include "calUtils.h"
#include "spefit.h"

/*---------------------------------------------------------------------------*/

void getBaseline(calib_data *dom_calib, float max_var, float base[2][3]) {

    short atwd, ch;
    int cnt = 128;

    int bin, trigger_mask, trig, wf_bad, wf_bad_cnt;
    float true_wf[3][128], mean, var;
    float var_avg;

    /* Channel readout buffers for each channel and bin */
    short channels[3][128];

    /* Baseline waveform arrays for each atwd, channel, and voltage setting */
    float baseline[2][3][128];

    /* Get bias DAC setting */
    short bias_dac = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    float bias_v = biasDAC2V(bias_dac);

    /* Iterate over each atwd */
    for (atwd = 0; atwd < 2; atwd++) {

        /* Initialize the baseline arrays */
        for (ch = 0; ch < 3; ch++) {
            for (bin = 0; bin < cnt; bin++)
                baseline[atwd][ch][bin] = 0.0;
        }
        var_avg = 0.0;
        
        /* Trigger one ATWD only */
        trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;
        
        trig = 0;
        wf_bad_cnt = 0;
        while (trig < BASELINE_CAL_TRIG_CNT) {
            
            wf_bad = 0;

            /* Warm up the ATWD */
            prescanATWD(trigger_mask);
   
            /* CPU-trigger the ATWD */
            hal_FPGA_TEST_trigger_forced(trigger_mask);
        
            /* Wait for done */
            while (!hal_FPGA_TEST_readout_done(trigger_mask));
            
            /* Read out one waveform for all channels except 3 */        
            if (atwd == 0) {
                hal_FPGA_TEST_readout(channels[0], channels[1], channels[2], NULL, 
                                      NULL, NULL, NULL, NULL,
                                      cnt, NULL, 0, trigger_mask);
            }
            else {
                hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                      channels[0], channels[1], channels[2], NULL,
                                      cnt, NULL, 0, trigger_mask);
            }
            
            /* For each bin, get the voltage */
            for (ch = 0; ch < 3; ch++) {
                for (bin = 0; bin < cnt; bin++) {
                    float v;
                    float val = (float)channels[ch][bin];
                    /* Using ATWD calibration data, convert to V */
                    if (atwd == 0) {
                        v = val * dom_calib->atwd0_gain_calib[ch][bin].slope
                            + dom_calib->atwd0_gain_calib[ch][bin].y_intercept;
                    }
                    else {
                        v = val * dom_calib->atwd1_gain_calib[ch][bin].slope
                            + dom_calib->atwd1_gain_calib[ch][bin].y_intercept;        
                    }
                    
                    /* Also subtract out bias voltage */
                    v -= bias_v;
                    true_wf[ch][bin] = v;
                }
                
                /* Check the variance */
                /* Note that we assume that if variance is OK for channel 0 */
                /* there is no contamination in any channel */
                if (ch == 0) {
                    meanVarFloat(true_wf[0], 128, &mean, &var);
                    
                    if (var > max_var) {
                        wf_bad = 1;
                        wf_bad_cnt++;
                        if (wf_bad_cnt > BASELINE_CAL_TRIG_CNT) {
                            max_var *= 1.5;
                            wf_bad_cnt = 0;
                        }
                        break;
                    }
                    else
                        var_avg += var;
                }
            }
            
            if (!wf_bad) {
                trig++;
                /* Variance is OK; record for average */
                for (ch = 0; ch < 3; ch++)
                    for (bin = 0; bin < cnt; bin++) 
                        baseline[atwd][ch][bin] += true_wf[ch][bin];                     
            }
            
        } /* End trigger loop */

        /* Get average variance of channel 0 */
        var_avg /= (float)BASELINE_CAL_TRIG_CNT;
        /* Get the average baseline waveform */
        for (ch = 0; ch < 3; ch++) {
            
            for(bin = 0; bin < cnt; bin++) {
                baseline[atwd][ch][bin] /= (float)BASELINE_CAL_TRIG_CNT;
            }
             
            /* Just get the mean */
            /* Don't use first 3 samples -- "nook" in baseline is not understood */
            meanVarFloat(baseline[atwd][ch]+3, cnt-3, &(base[atwd][ch]), &var);
        }    
    }               
}

/*---------------------------------------------------------------------------*/

int baseline_cal(calib_data *dom_calib) {

    /* Baseline results for each atwd and channel */
    float baselines[2][3];

#ifdef DEBUG
    printf("Performing baseline calibration...\r\n");
#endif

    /* Disable the analog mux */
    /* THIS AFFECTS THE BASELINE */
    halDisableAnalogMux();

    /* Don't need to worry about variance */
    float max_var = 1.0;

    /* Find the baselines */
    getBaseline(dom_calib, max_var, baselines);

    /* Record non-HV baselines into result structure */
    int ch;
    for (ch = 0; ch < 3; ch++) {
        dom_calib->atwd0_baseline[ch] = baselines[0][ch];
        dom_calib->atwd1_baseline[ch] = baselines[1][ch];
    }

    return 0;
}

/*---------------------------------------------------------------------------*/

int hv_baseline_cal(calib_data *dom_calib) {

    /* Baseline results for each HV, atwd, and channel */
    float baselines[GAIN_CAL_HV_CNT][2][3];

    /* Static results for structure */
    static hv_baselines hv_base_data[GAIN_CAL_HV_CNT];

#ifdef DEBUG
    printf("Performing HV baseline calibration...\r\n");
#endif

    /* Disable the analog mux */
    /* THIS AFFECTS THE BASELINE */
    halDisableAnalogMux();

    /* Turn on high voltage base */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halEnablePMT_HV();
#else
    halPowerUpBase();
    halEnableBaseHV();
#endif

    /* Ensure HV base exists before performing calibration */
    if (!checkHVBase()) {
        dom_calib->hv_baselines_valid = 0;
        return 0;
    }

    short hv_idx = 0;
    short hv;

    /* Loop over HV settings */
    for (hv_idx = 0; hv_idx < GAIN_CAL_HV_CNT; hv_idx++) {
        
        /* Set high voltage and give it time to stabilize */
        hv = (hv_idx * GAIN_CAL_HV_INC) + GAIN_CAL_HV_LOW;
        
        /* Check that we're not exceeding maximum requested HV */
        if (hv > dom_calib->max_hv) {
#ifdef DEBUG
            printf("HV of %dV higher than requested maximum, skipping...\r\n", hv);
#endif
            continue;
        }

#ifdef DEBUG
        printf(" Setting HV to %d V\r\n", hv);
#endif
        
        halWriteActiveBaseDAC(hv * 2);
        halUSleep(5000000);

        /* Starting maximum variance allowed for SPE exclusion */
        float max_var = BASELINE_CAL_MAX_VAR;

        getBaseline(dom_calib, max_var, baselines[hv_idx]);
       
        /* Store results */
        hv_base_data[hv_idx].voltage = hv; 
        int ch;
        for (ch = 0; ch < 3; ch++) {
            hv_base_data[hv_idx].atwd0_hv_baseline[ch] = baselines[hv_idx][0][ch];
            hv_base_data[hv_idx].atwd1_hv_baseline[ch] = baselines[hv_idx][1][ch];
        }

        dom_calib->num_baselines++;
    }

    dom_calib->baseline_data = hv_base_data;
    dom_calib->hv_baselines_valid = 1;

    return 0;

}
