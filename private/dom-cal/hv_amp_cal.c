/*
 * hv_amp_cal
 *
 * Amplifier recalibration routine -- record waveform peaks in 
 * ATWD channels for a muons and use ATWD calibration to find 
 * gain of each amplifier.  Uses only one ATWD.
 *
 */

#include <stdio.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "amp_cal.h"
#include "hv_amp_cal.h"
#include "calUtils.h"

/*---------------------------------------------------------------------------*/
int hv_amp_cal(calib_data *dom_calib) {

    const int cnt = 128;
    int trigger_mask;
    int ch, bin, trig;
    float bias_v, peak_v, hpeak_v;

    int hv;
    
    /* Which atwd to use */
    short atwd = AMP_CAL_ATWD;

    /* Channel readout buffers for each channel and bin */
    /* This test only uses one ATWD */
    short channels[3][128];

    /* Peak arrays for each channel, in Volts */
    float peaks[3][AMP_CAL_TRIG_CNT];

    /* Peak arrays for higher gain channels, for later comparison, in Volts */
    float hpeaks[3][AMP_CAL_TRIG_CNT];

    

#ifdef DEBUG
    printf("Performing HV amplifier calibration (using ATWD%d)...\r\n", atwd);
#endif

    /* Save DACs that we modify */
    short origBiasDAC = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);
    /* Increase sampling speed to make sure we see peak */
    short origSampDAC = halReadDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                                   DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);

    /* Set discriminator and bias level */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, AMP_CAL_PEDESTAL_DAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, HV_AMP_CAL_DISC_DAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, AMP_CAL_SAMPLING_DAC);

    bias_v = biasDAC2V(AMP_CAL_PEDESTAL_DAC);

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

    hv = HV_AMP_CAL_VOLTS;
                                                                                                                                               
#ifdef DEBUG
    printf(" Setting HV to %d V\r\n", hv);
#endif
                                                                                                                                               
    halWriteActiveBaseDAC(hv * 2);
    halUSleep(5000000);

    /* Find the proper hv_baselines for this voltage */
    hv_baselines *hv_baseline = NULL;
   
    int i;
    for(i = 0; i < calib_data->num_histos; i++) {
        if (calib_data->baseline_data[i].voltage == hv) hv_baselines = baseline_data[i];
    }

    if (hv_baseline == NULL) {
        printf("HV Baseline calibration data not found...aborting");
        return -1;
    }
    
    /* Trigger one ATWD only */
    trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Loop over channels and pulser settings for each channel */
    for (ch = 1; ch < 3; ch++) {

        for (trig=0; trig<(int)AMP_CAL_TRIG_CNT; trig++) {
                
            /* Warm up the ATWD */
            prescanATWD(trigger_mask);
            
            /* Discriminator trigger the ATWD */
            hal_FPGA_TEST_trigger_disc(trigger_mask);
            
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

            /* Look at raw ATWD data of higher gain channel, which should be
             * calibrated already, to check range
             */
            short max = 0;
            for (bin = 0; bin < cnt; bin++) max = channels[ch-1][bin] > max ? channels[ch-1][bin] : max;
            if (max < HV_AMP_CAL_MIN_PULSE || max > HV_AMP_CAL_MAX_PULSE) {
                trig--;
                continue;
            }
            
            /* OK -- we have a reasonable muon pulse */
            
            /* FIX ME */
            printf("Yay! Just got trigger %d \n", trig);

            /* Find and record peak.  Make sure it is in range */ 
            for (bin=AMP_CAL_START_BIN; bin<cnt; bin++) {


                /* Using ATWD calibration data, convert to actual V */
                if (atwd == 0) {
                    peak_v = (float)(channels[ch][bin]) * dom_calib->atwd0_gain_calib[ch][bin].slope
                        + dom_calib->atwd0_gain_calib[ch][bin].y_intercept
                        - hv_baseline->atwd0_baseline[ch];
                    hpeak_v = (float)(channels[ch-1][bin]) * dom_calib->atwd0_gain_calib[ch-1][bin].slope
                        + dom_calib->atwd0_gain_calib[ch-1][bin].y_intercept
                        - hv_baseline->atwd0_baseline[ch-1];
                }
                else {
                    peak_v = (float)(channels[ch][bin]) * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept
                        - hv_baseline->atwd1_baseline[ch];
                    hpeak_v = (float)(channels[ch-1][bin]) * dom_calib->atwd1_gain_calib[ch-1][bin].slope
                        + dom_calib->atwd1_gain_calib[ch-1][bin].y_intercept
                        - hv_baseline->atwd1_baseline[ch-1];
                }

                /* Also subtract out bias voltage */
                peak_v -= bias_v;
                hpeak_v -= bias_v;

                /* Note "peak" is actually a minimum */
                if (bin == 0) {
                    peaks[ch][trig] = peak_v;
                    hpeaks[ch-1][trig] = hpeak_v;
                }
                else {
                    peaks[ch][trig] = (peak_v < peaks[ch][trig]) ? 
                        peak_v : peaks[ch][trig];
                    hpeaks[ch-1][trig] = (hpeak_v < hpeaks[ch-1][trig]) ?
                        hpeak_v : hpeaks[ch-1][trig];
                }
            }
        }
    }


    float mean, var;
    for (ch = 1; ch < 3; ch++) {
        /* Divide by peaks in higher gain ch to get amp ratio */
        /* Multiply by known amplification factor to find gain */
        for (bin = 0; bin < AMP_CAL_TRIG_CNT; bin++) {
            peaks[ch][bin] /= hpeaks[ch-1][bin];
            peaks[ch][bin] *= dom_calib->amplifier_calib[ch-1].value;
        }

        /* Find mean and var */
        meanVarFloat(peaks[ch], AMP_CAL_TRIG_CNT, &mean, &var);

        /* FIX ME */
        printf("Old gain for ch %d: %g\r\n", ch, dom_calib->amplifier_calib[ch].value);


        dom_calib->amplifier_calib[ch].value = mean;
        dom_calib->amplifier_calib[ch].error = var;

        /* FIX ME */
        printf("Gain for ch %d: %g\r\n", ch, (mean));

    }

    /* Put the DACs back to original state */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);

    /* Won't turn off the HV for now...*/

    /* FIX ME: return real error code */
    return 0;

}
