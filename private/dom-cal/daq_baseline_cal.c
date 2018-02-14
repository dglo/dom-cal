/*
 * daq_baseline
 *
 * Measure the baseline after calibration, using the DAQ FPGA
 * and settings to mimic DOM state during data-taking.
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
#include "daq_baseline_cal.h"

int daq_baseline_cal(calib_data *dom_calib) {

    int cnt = 128;

    int atwd, ch, bin;
    int npeds0, npeds1;

#ifdef DEBUG
    printf("Performing DAQ baseline calibration...\r\n");
#endif
    
    /* Load the DOMApp FPGA if it's not already loaded */
    if (is_domapp_loaded() == 0) {
        if (load_domapp_fpga() != 0) {
            printf("Error loading domapp FPGA... aborting DAQ baseline calibration\r\n");
            return 1;
        }
    }

    /*------------------------------------------------------------------*/
    /* The following is adapted from domapp's pedestalRun()             */

    /* Zero the running baseline sum */
    for(ch=0; ch<3; ch++) {
        for(bin=0; bin<cnt; bin++) {
            dom_calib->atwd0_daq_baseline_wf[ch][bin] = 0;
            dom_calib->atwd1_daq_baseline_wf[ch][bin] = 0;
        }
    }
    /* dsc_hal_disable_LC_completely(); */
    hal_FPGA_DOMAPP_disable_daq();

    /* Reset and zero the LBM */
    hal_FPGA_DOMAPP_lbm_reset();
    halUSleep(1000);
    hal_FPGA_DOMAPP_lbm_reset(); /* really! */
    memset(hal_FPGA_DOMAPP_lbm_address(), 0, WHOLE_LBM_MASK+1);
    unsigned lbmp = hal_FPGA_DOMAPP_lbm_pointer();

    /* Set up forced trigger mode */
    hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FORCED);
    hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
    hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_FORCED);
    hal_FPGA_DOMAPP_daq_mode(HAL_FPGA_DOMAPP_DAQ_MODE_ATWD_FADC);
    hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_TESTING);
    hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_WRAP);
    hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_OFF);
    hal_FPGA_DOMAPP_rate_monitor_enable(0);

    /* LC mode on / off */
    hal_FPGA_DOMAPP_lc_mode(DAQ_BASELINE_LC_ON ? 
                            HAL_FPGA_DOMAPP_LC_MODE_HARD :
                            HAL_FPGA_DOMAPP_LC_MODE_OFF);
#ifdef DEBUG
    if (DAQ_BASELINE_LC_ON) {
        printf("Local coincidence enabled.\r\n");
    }
#endif

    /* HV on / off */
    if (DAQ_BASELINE_HV_ON) {
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
        halEnablePMT_HV();
#else
        halPowerUpBase();
        halEnableBaseHV();
#endif
        
        short hv = DAQ_BASELINE_HV;
		//if possible use the voltage calibrated to give a gain of 10^7, as during data taking
		if(dom_calib->hv_gain_valid && dom_calib->hv_gain_calib.r_squared>.99)
			hv = pow(10.0,(7.0-dom_calib->hv_gain_calib.y_intercept)/dom_calib->hv_gain_calib.slope);
        
        /* Check to make sure we're not exceeding min or max requested HV */
        hv = (hv > dom_calib->max_hv) ? dom_calib->max_hv : hv;
		hv = (hv < dom_calib->min_hv) ? dom_calib->min_hv : hv;

#ifdef DEBUG
        printf(" Setting HV to %d V\r\n", hv);
#endif
        
        halWriteActiveBaseDAC(hv * 2);
        halUSleep(5000000);
    }

    int didMissedTrigWarning   = 0;
    npeds0 = npeds1            = 0;

    /*--------------------------------------------------------------------------*/
    /* Loop over atwds */
    for(atwd=0;atwd<2;atwd++) {

        int numMissedTriggers = 0;

        /* Discard first couple of triggers */
        int numTrigs = DAQ_BASELINE_TRIG_CNT + 2;

        hal_FPGA_DOMAPP_enable_atwds(atwd==0?HAL_FPGA_DOMAPP_ATWD_A:HAL_FPGA_DOMAPP_ATWD_B);
        hal_FPGA_DOMAPP_enable_daq(); 

        int trial,
            maxtrials = 10,
            trialtime = DAQ_BASELINE_WAIT_US,
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

        /* Iterate back through triggers -- make sure there are no stray pulses */
        /* in case HV is on */       

        /* Get bias DAC setting */
        short bias_dac = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
        float bias_v = biasDAC2V(bias_dac);

        float baseline[2][4][128];
        for(trig=0; trig<(numTrigs-numMissedTriggers); trig++) {

            /* Discard the first couple of triggers */
            if (trig >= 2) {

                /* Get lbm event */
                unsigned char * e = ((unsigned char *)hal_FPGA_DOMAPP_lbm_address()) + (lbmp & WHOLE_LBM_MASK);
                unsigned short * atwd_data = (unsigned short *) (e+0x210);
                
                /* Count the waveforms into the running sums */
                for(ch=0; ch<3; ch++) {
                    for(bin=0; bin<cnt; bin++) {
                        float v;
                        float val = (float)(atwd_data[cnt*ch + bin] & 0x3FF);
                        
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
                        baseline[atwd][ch][bin] = v;
                    }
                }
                
                /* Calculate average and check ch0 variance */
                float mean, var;
                meanVarFloat(baseline[atwd][0], 128, &mean, &var);
                
                /* Variance is OK */
                if (var < DAQ_BASELINE_MAX_VAR) {
                    for(ch=0; ch<3; ch++) {
                        for(bin=0; bin<cnt; bin++) {
                            if (atwd == 0)
                                dom_calib->atwd0_daq_baseline_wf[ch][bin] += baseline[atwd][ch][bin];
                            else
                                dom_calib->atwd1_daq_baseline_wf[ch][bin] += baseline[atwd][ch][bin];
                        }
                    }

                    if(atwd==0) npeds0++; else npeds1++;
                }
            }
            
            /* Go to next event in LBM */
            lbmp = lbmp + HAL_FPGA_DOMAPP_LBM_EVENT_SIZE;
            
        } /* Variance check waveform loop */

        /* Make sure we have enough waveforms */
        int npeds = atwd==0 ? npeds0 : npeds1;
        if (npeds < DAQ_BASELINE_MIN_WF) {
            printf("ERROR: Number of good waveforms for atwd %d fewer than minimum! ", atwd);
            printf("(%d good, %d required)\r\n", npeds, DAQ_BASELINE_MIN_WF);
            return 1;
        }

        /* Normalize the sums */
        for(ch=0; ch<3; ch++) {            
            for(bin=0; bin<cnt; bin++) {
                if (atwd == 0)
                    dom_calib->atwd0_daq_baseline_wf[ch][bin] /= (float)npeds;
                else
                    dom_calib->atwd1_daq_baseline_wf[ch][bin] /= (float)npeds;
            }
        }   
    } /* loop over atwds */
    
    /*------------------------------------------------------------------*/
    /* Stop data taking */
    
    hal_FPGA_DOMAPP_disable_daq();
    hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_OFF);

    /*------------------------------------------------------------------*/

    /* Calculate averages */
    float base[2][3];
    float var;

    /* Mark calibration valid */
    dom_calib->daq_baselines_valid = 1;

    /* This is all extraneous */
    for (atwd = 0; atwd<2; atwd++)
        /* Don't use first 3 samples -- "nook" in baseline is not understood */
        /* OK, really, it was an FPGA problem that's supposedly gone, but I'm still paranoid */
        for(ch=0; ch<3; ch++) {
            if (atwd == 0)
                meanVarFloat(dom_calib->atwd0_daq_baseline_wf[ch]+3, cnt-3, &(base[atwd][ch]), &var);
            else
                meanVarFloat(dom_calib->atwd1_daq_baseline_wf[ch]+3, cnt-3, &(base[atwd][ch]), &var);
                
            float origbase = (atwd == 0) ?  
                dom_calib->atwd0_baseline[ch] :
                dom_calib->atwd1_baseline[ch] ;
                
#ifdef DEBUG
            printf("ATWD %d ch %d: %.3f mV  orig: %.3f mV\r\n", atwd, ch, base[atwd][ch]*1000, origbase*1000);
#endif
        }

    return 0;

}
