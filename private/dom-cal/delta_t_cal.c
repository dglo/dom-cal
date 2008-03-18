/*
 * delta_t_cal
 *
 * Measure time offset between ATWD A, ATWD B, and FADC features
 * using the front-end pulser.  Uses domapp FPGA, because FADC
 * capture window is different than STF FPGA.
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_FPGA_domapp_regs.h"

#include "domcal.h"
#include "calUtils.h"
#include "zlib.h"
#include "icebootUtils.h"
#include "delta_t_cal.h"

int delta_t_cal(calib_data *dom_calib) {

    int cnt = 128;
    int fadc_cnt = 256;

    int ch, bin;
    int npeds;
    
    /* Which atwd to use for time reference */
    /* For a consistent calibration, this has to match transit time calibration ATWD */
    short atwd = DELTA_T_ATWD;
    
    /* Calculate offset for other atwd */
    short atwd2 = DELTA_T_ATWD ^ 0x1;

    /* ATWD and FADC baselines */
    float ch0_baseline_wf[2][128];
    float fadc_baseline;

    /* Average FADC waveform */
    float fadc_avg[256];

    /* Average ATWD waveforms */
    float atwd_avg[2][128];

    /* Time offset between leading edges */
    float fadc_delta_t[DELTA_T_PULSER_AMP_CNT];
    float atwd_delta_t[DELTA_T_PULSER_AMP_CNT];

#ifdef DEBUG
    printf("Performing ATWD/FADC delta_t calibration...\r\n");
#endif
    
    /* Load the DOMApp FPGA if it's not already loaded */
    if (is_domapp_loaded() == 0) {
        if (load_domapp_fpga() != 0) {
            printf("Error loading domapp FPGA... aborting DAQ baseline calibration\r\n");
            return 1;
        }
    }

    /* Make sure the HV is OFF */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halDisablePMT_HV();
#else
    halDisableBaseHV();
    halPowerDownBase();
#endif
    halUSleep(250000);

    /* Increase sampling speed to make sure we see peak */
    short origSampDAC[2];
    origSampDAC[0] = halReadDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS);
    origSampDAC[1] = halReadDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);
    short origBiasDAC = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);
    short origFADCRef = halReadDAC(DOM_HAL_DAC_FAST_ADC_REF);

    halWriteDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS, DELTA_T_SAMPLING_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, DELTA_T_SAMPLING_DAC);
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, DELTA_T_DISC_DAC);
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, DELTA_T_PEDESTAL_DAC);   
    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, DELTA_T_FADC_REF);       

    halUSleep(DAC_SET_WAIT);

    /*------------------------------------------------------------------*/
    /* Zero the running sums */
    int a;
    for (a = 0; a < 2; a++) {
        for(bin=0; bin<cnt; bin++) {
            ch0_baseline_wf[a][bin] = 0;
            atwd_avg[a][bin] = 0;
        }
    }
    for (bin=0; bin<fadc_cnt; bin++) 
        fadc_avg[bin] = 0;
    fadc_baseline = 0;

    /* Sampling frequencies */
    float freq[2];
    freq[0] = getCalibFreq(0, *dom_calib, DELTA_T_SAMPLING_DAC);
    freq[1] = getCalibFreq(1, *dom_calib, DELTA_T_SAMPLING_DAC);
    float fadc_freq = FADC_CLOCK_FREQ;           

    /*------------------------------------------------------------------*/
    /* We do the DAQ loop multiple times                                
     *  - first record baselines (both ATWDs triggered)
     *  - record average waveforms for different pulser amplitudes
     */

    hal_FPGA_DOMAPP_disable_daq();

    int pulser_loop = 0;
    int pulser_idx = 0;
    int pulser_good_cnt = 0;
    while (pulser_idx < DELTA_T_PULSER_AMP_CNT) {

        /* Reset and zero the LBM */
        hal_FPGA_DOMAPP_lbm_reset();
        halUSleep(1000);
        hal_FPGA_DOMAPP_lbm_reset(); /* really! */
        memset(hal_FPGA_DOMAPP_lbm_address(), 0, WHOLE_LBM_MASK+1);
        unsigned lbmp = hal_FPGA_DOMAPP_lbm_pointer();

        if (pulser_loop == 0) {
            /* Set up forced trigger mode */           
            hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FORCED);
            hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
            hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_FORCED);
        }
        else {
            /* Now turn on the pulser and acquire average pulser waveforms     */
            /* Set the pulser rate to something reasonable */
            hal_FPGA_DOMAPP_cal_pulser_rate(DELTA_T_PULSER_RATE_HZ);

            /* Set the pulser amplitude */
            int pulser_dac = DELTA_T_PULSER_AMP_MIN + pulser_idx*DELTA_T_PULSER_AMP_STEP;
#ifdef DEBUG
            printf("Setting pulser dac to %d\r\n", pulser_dac);
#endif
            halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, pulser_dac);

            /* Enable pulser calibration triggers */
            hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_SPE);
            hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FE_PULSER);
            hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_REPEAT);
        }

        hal_FPGA_DOMAPP_daq_mode(HAL_FPGA_DOMAPP_DAQ_MODE_ATWD_FADC);
        hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_TESTING);
        hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_WRAP);
        hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_OFF);
        hal_FPGA_DOMAPP_rate_monitor_enable(0);
        hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_OFF);
        
        int didMissedTrigWarning   = 0;
        npeds                      = 0;
        int numMissedTriggers = 0;
        
        /* Discard first couple of triggers */
        int numTrigs = DELTA_T_TRIG_CNT + 2;
        
        /* Enable BOTH ATWDs */
        hal_FPGA_DOMAPP_enable_atwds(HAL_FPGA_DOMAPP_ATWD_A|HAL_FPGA_DOMAPP_ATWD_B);
        hal_FPGA_DOMAPP_enable_daq(); 
        
        int trial,
            maxtrials = 10,
            trialtime = DELTA_T_WAIT_US,
            done      = 0;
        
        /* Get number of triggers requested */
        int trig; 
        for(trig=0; trig<numTrigs; trig++) {
            
            hal_FPGA_DOMAPP_cal_launch();
            done = 0;
            for(trial=0; !done && trial<maxtrials; trial++) {
                halUSleep(trialtime); /* Give FPGA time to write LBM without CPU hogging the bus */
                if(hal_FPGA_DOMAPP_lbm_pointer() >= lbmp+FPGA_DOMAPP_LBM_BLOCKSIZE*(trig+1)) done = 1;
            }
            
            if(!done) {
                numMissedTriggers++;
                if(!didMissedTrigWarning) {
                    didMissedTrigWarning++;
                    printf("WARNING: missed one or more calibration triggers for ATWD %d! "
                           "lbmp=0x%08x fpga_ptr=0x%08x", atwd, lbmp, hal_FPGA_DOMAPP_lbm_pointer());
                }
            }
        } /* Data-taking trigger loop */
        
        /*------------------------------------------------------------------*/
        /* Iterate back through triggers and extract baselines */
        
        /* Get bias DAC setting */
        short bias_dac = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
        float bias_v = biasDAC2V(bias_dac);
        
        for(trig=0; trig<(numTrigs-numMissedTriggers); trig++) {
            
            /* Discard the first couple of triggers */
            if (trig >= 2) {
                
                /* Get lbm event */
                unsigned char * e = ((unsigned char *)hal_FPGA_DOMAPP_lbm_address()) + (lbmp & WHOLE_LBM_MASK);
                unsigned short * fadc_data = (unsigned short *) (e+0x10);
                unsigned short * atwd_data = (unsigned short *) (e+0x210);
                
                /* Count the waveforms into the running sums */
                ch = 0;
                float val;
                for(bin=0; bin<cnt; bin++) {
                    val = (float)(atwd_data[cnt*ch + bin] & 0x3FF);
                    
                    /* Using ATWD calibration data, convert to V */
                    if (pulser_loop == 0) {
                        ch0_baseline_wf[0][bin] += val * dom_calib->atwd0_gain_calib[ch][bin].slope
                            + dom_calib->atwd0_gain_calib[ch][bin].y_intercept - bias_v;
                        
                        ch0_baseline_wf[1][bin] += val * dom_calib->atwd1_gain_calib[ch][bin].slope
                            + dom_calib->atwd1_gain_calib[ch][bin].y_intercept - bias_v;
                    }
                    else {
                        atwd_avg[0][bin] += val * dom_calib->atwd0_gain_calib[ch][bin].slope
                            + dom_calib->atwd0_gain_calib[ch][bin].y_intercept - bias_v
                            - ch0_baseline_wf[0][bin];
                        
                        atwd_avg[1][bin] += val * dom_calib->atwd1_gain_calib[ch][bin].slope
                            + dom_calib->atwd1_gain_calib[ch][bin].y_intercept - bias_v
                            - ch0_baseline_wf[1][bin];
                    }
                }

                for(bin=0; bin<fadc_cnt; bin++) {
                    if (pulser_loop == 0)
                        fadc_baseline += fadc_data[bin] & 0x3FF;
                    else
                        fadc_avg[bin] += (fadc_data[bin] & 0x3FF) - fadc_baseline;
                }
            }        
            npeds++;
            
            /* Go to next event in LBM */
            lbmp = lbmp + HAL_FPGA_DOMAPP_LBM_EVENT_SIZE;
            
        } /* Baseline waveform loop */
        
        /* Make sure we have enough waveforms */
        if (npeds < DELTA_T_MIN_WF) {
            printf("ERROR: Number of good waveforms fewer than minimum! ");
            printf("(%d good, %d required)\r\n", npeds, DELTA_T_MIN_WF);
            return 1;
        }
        
        /* Normalize the sums */
        for(a = 0; a < 2; a++) {
            for(bin=0; bin<cnt; bin++) {
                if (pulser_loop == 0)
                    ch0_baseline_wf[a][bin] /= (float)npeds * dom_calib->amplifier_calib[0].value;
                else 
                    atwd_avg[a][bin] /= (float)npeds * dom_calib->amplifier_calib[0].value;
            }
        }
        if (pulser_loop == 0) {
            /* FADC baseline is just a constant offset, we don't save whole waveform */
            fadc_baseline /= (float)npeds*fadc_cnt;
        }
        else {
            for(bin=0; bin<fadc_cnt; bin++) {
                fadc_avg[bin] /= (float)npeds;
                fadc_avg[bin] *= dom_calib->fadc_gain.value;
            }
        }

        if (pulser_loop != 0) {

            /*------------------------------------------------------------------*/
            /* Analyze average waveforms for time offset */
            
            /* Get sampling speed frequency in MHz for both ATWDs */    
            float le_x[128], le_y[128];        
            int le_cnt;
            linear_fit le_fit;
            float atwd_le[2], fadc_le;
            atwd_le[0] = atwd_le[1] = fadc_le = 0.0;
            int bad = 0;
            
            /* Get ATWD LE time */
            int peak_idx;
            float peak_v;
            for (a = 0; a < 2; a++) {
                peak_idx = DELTA_T_START_BIN;
                peak_v = atwd_avg[a][DELTA_T_START_BIN];
                for (bin=DELTA_T_START_BIN; bin<cnt; bin++) {
                    if (atwd_avg[a][bin] > peak_v) {
                        peak_v = atwd_avg[a][bin];
                        peak_idx = bin;
                    }
                }
                
                /* Use peak and points down to 10% for LE */
                le_cnt = 0;
                for (bin=peak_idx; bin<cnt; bin++) {
                    le_x[le_cnt] = (cnt-bin-1)*1000.0/freq[a];
                    le_y[le_cnt] = atwd_avg[a][bin];
                    le_cnt++;
                    if (atwd_avg[a][bin] < 0.1*peak_v) 
                        break;
                }
                
                /* Fit LE */
                if (le_cnt >= 2) {
                    linearFitFloat(le_x, le_y, le_cnt, &le_fit);
                    atwd_le[a] = -le_fit.y_intercept / le_fit.slope;                    
                }
                else {
#ifdef DEBUG
                    printf("WARNING: too few points to fit ATWD%d leading edge, discarding!\r\n", a);
#endif
                    bad = 1;
                }
            }
            
            /* This could have a significant systematic offset */
            /* The FADC LE is extremely poor */
            /* Get FADC LE time */
            peak_idx = 0;
            peak_v = fadc_avg[0];
            for (bin=0; bin<DELTA_T_STOP_SAMPLE; bin++) {
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
                printf("WARNING: too few points to fit FADC leading edge, discarding!\r\n");
#endif
                bad = 1;
            }
            
            /* Now compute offset assuming LE are in same position */
            if (!bad) {
                fadc_delta_t[pulser_good_cnt] = atwd_le[atwd] - fadc_le;
                atwd_delta_t[pulser_good_cnt] = atwd_le[atwd] - atwd_le[atwd2];            

#ifdef DEBUG
                /* TEMP FIX ME */
                printf("Offset between ATWD%d and FADC leading edges: %.1f ns\r\n", 
                       atwd, fadc_delta_t[pulser_good_cnt]);
                printf("Offset between ATWD%d and ATWD%d leading edges: %.1f ns\r\n", 
                       atwd, atwd2, atwd_delta_t[pulser_good_cnt]);
#endif   
                pulser_good_cnt++;
            }
        }

        /* Increment to pulser loop */
        if (pulser_loop == 0)
            pulser_loop++;
        else /* Increment pulser amplitude index */
            pulser_idx++;
        
    } /* End DAQ / pulser settings loop */

    /*------------------------------------------------------------------*/
    /* Calculate mean / error and save calibration results */
    
    /* Check that we had good results */
    if (pulser_good_cnt == 0) {
        dom_calib->fadc_delta_t.value = 0;
        dom_calib->fadc_delta_t.error = 0;

        dom_calib->atwd_delta_t[atwd].value = 0;
        dom_calib->atwd_delta_t[atwd].error = 0;

        dom_calib->atwd_delta_t[atwd2].value = 0;
        dom_calib->atwd_delta_t[atwd2].error = 0;

        printf("ERROR: no good delta_t points -- all offsets set at zero!!!\r\n");
        return -1;
    }

    /* Calculate FADC delta_t average over all pulser settings */
    float fadc_delta_t_mean, fadc_delta_t_var;
    meanVarFloat(fadc_delta_t, pulser_good_cnt, &fadc_delta_t_mean, &fadc_delta_t_var);
    dom_calib->fadc_delta_t.value = fadc_delta_t_mean;
    dom_calib->fadc_delta_t.error = sqrt(fadc_delta_t_var / pulser_good_cnt);

    /* Calculate ATWD2 delta_t average over all pulser settings */
    float atwd_delta_t_mean, atwd_delta_t_var;
    meanVarFloat(atwd_delta_t, pulser_good_cnt, &atwd_delta_t_mean, &atwd_delta_t_var);

    /* Sign of correction depends on whether we used same ATWD as transit time calibration */
    /* Goal is just to allow users to add transit time number with this delta, without */
    /* knowing which ATWD was used for what */
    if ((!dom_calib->transit_calib_valid) || (atwd == dom_calib->transit_calib_atwd)) {
        dom_calib->atwd_delta_t[atwd].value = 0;
        dom_calib->atwd_delta_t[atwd].error = 0;

        dom_calib->atwd_delta_t[atwd2].value = atwd_delta_t_mean;
        dom_calib->atwd_delta_t[atwd2].error = sqrt(atwd_delta_t_var / pulser_good_cnt);
    }
    else {
        dom_calib->atwd_delta_t[atwd2].value = 0;
        dom_calib->atwd_delta_t[atwd2].error = 0;

        dom_calib->atwd_delta_t[atwd].value = -atwd_delta_t_mean;
        dom_calib->atwd_delta_t[atwd].error = sqrt(atwd_delta_t_var / pulser_good_cnt);
    }

#ifdef DEBUG
    printf("Mean ATWD%d offset: %.2f +/- %.2f ns\r\n", atwd2, atwd_delta_t_mean, 
           sqrt(atwd_delta_t_var / pulser_good_cnt));
#endif

    /*
    printf("Calibrated waveforms with time offsets included (ns mV)\r\n");
    printf("ATWD0\r\n");
    for (bin=cnt-1; bin>=0; bin--)
        printf("%g %g\r\n", (cnt-1-bin)*1000.0/freq[0], atwd_avg[0][bin]*1000.0);
    printf("ATWD1\r\n");
    for (bin=cnt-1; bin>=0; bin--)
        printf("%g %g\r\n", (cnt-1-bin)*1000.0/freq[1] + atwd_delta_t_mean, atwd_avg[1][bin]*1000.0);
    printf("FADC\r\n");
    for (bin=0; bin<fadc_cnt; bin++)
        printf("%g %g\r\n", bin*1000.0/fadc_freq + fadc_delta_t_mean, fadc_avg[bin]*1000.0);
    */
    /*------------------------------------------------------------------*/
    /* Stop data taking */
    
    hal_FPGA_DOMAPP_disable_daq();
    hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_OFF);
    
    /* Return DACs to original setting */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
    halWriteDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS, origSampDAC[0]);
    halWriteDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC[1]);
    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, origFADCRef);

    return 0;

}
