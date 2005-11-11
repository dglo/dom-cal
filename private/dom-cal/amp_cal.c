/*
 * amp_cal
 *
 * Amplifier calibration routine -- record waveform peaks in 
 * ATWD channels for a given pulser amplitude, and use pulser
 * and ATWD calibration to find gain of each amplifier.  Uses
 * only one ATWD.
 *
 */

#include <stdio.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "amp_cal.h"
#include "calUtils.h"
#include "baseline_cal.h"

/*---------------------------------------------------------------------------*/
int amp_cal(calib_data *dom_calib) {

    const int cnt = 128;
    int trigger_mask;
    int ch, bin, trig, peak_idx, int_min, int_max;
    float bias_v, peak_v, v;
    
    /* Which atwd to use */
    short atwd = AMP_CAL_ATWD;

    /* Pulser amplitude settings for each channel */
    const int pulser_settings[3] = {AMP_CAL_PULSER_AMP_0, 
                                    AMP_CAL_PULSER_AMP_1, 
                                    AMP_CAL_PULSER_AMP_2 };

    /* Channel readout buffers for each channel and bin */
    /* This test only uses one ATWD */
    short channels[3][128];

    /* Peak and charge arrays for each channel, in Volts */
    float peaks[3][AMP_CAL_TRIG_CNT];
    float charges[3][AMP_CAL_TRIG_CNT];

#ifdef DEBUG
    printf("Performing amplifier calibration (using ATWD%d)...\r\n", atwd);
#endif

    /* Save DACs that we modify */
    short origBiasDAC = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);
    /* Increase sampling speed to make sure we see peak */
    short origSampDAC = halReadDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                                   DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);

    /* Set discriminator and bias level */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, AMP_CAL_PEDESTAL_DAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, AMP_CAL_DISC_DAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, AMP_CAL_SAMPLING_DAC);

    bias_v = biasDAC2V(AMP_CAL_PEDESTAL_DAC);

    /* Turn on the pulser and slow down the frequency a bit */
    hal_FPGA_TEST_enable_pulser();
    hal_FPGA_TEST_set_pulser_rate(DOM_HAL_FPGA_PULSER_RATE_1_2k);
    halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, 0);
    halUSleep(DAC_SET_WAIT);

    /* Recheck the ATWD baseline -- will ignore any stray pulses */
    /* Important -- shift is ~5mV */
    float baseline[2][3];
    getBaseline(dom_calib, BASELINE_CAL_MAX_VAR, baseline);

    /* Trigger one ATWD only */
    trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /*---------------------------------------------------------------------------*/
    /* Loop over channels and pulser settings for each channel */
    for (ch = 0; ch < 3; ch++) {

        /* Set the pulser amplitude */
        halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, pulser_settings[ch]);
        halUSleep(DAC_SET_WAIT);

        for (trig=0; trig < AMP_CAL_TRIG_CNT; trig++) {
                
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
            
            /* Find and record the peak for the channel we're calibrating */
            peak_idx = AMP_CAL_START_BIN;
            for (bin=AMP_CAL_START_BIN; bin<cnt; bin++) {

                /* Using ATWD calibration data, convert to actual V */
                if (atwd == 0) {
                    peak_v = (float)(channels[ch][bin]) * dom_calib->atwd0_gain_calib[ch][bin].slope
                        + dom_calib->atwd0_gain_calib[ch][bin].y_intercept
                        - baseline[atwd][ch];
                }
                else {
                    peak_v = (float)(channels[ch][bin]) * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept
                        - baseline[atwd][ch];
                }

                /* Also subtract out bias voltage */
                peak_v -= bias_v;

                /* Note "peak" is actually a minimum */
                if (bin == AMP_CAL_START_BIN)
                    peaks[ch][trig] = peak_v;
                else {
                    if (peak_v < peaks[ch][trig]) {
                        peaks[ch][trig] = peak_v;
                        peak_idx = bin;
                    }
                }                
            } /* End peak loop */

            /* Now integrate the charge */
            charges[ch][trig] = v = 0.0;       
            
            int_min = (peak_idx - AMP_CAL_INT_WIN_MIN >= AMP_CAL_START_BIN) ?
                peak_idx - AMP_CAL_INT_WIN_MIN : AMP_CAL_START_BIN;
            int_max = (peak_idx + AMP_CAL_INT_WIN_MAX <= cnt-1) ? 
                peak_idx + AMP_CAL_INT_WIN_MAX : cnt-1;

            for (bin = int_min; bin <= int_max; bin++) {
                if (atwd == 0) {
                    v = (float)(channels[ch][bin]) * dom_calib->atwd0_gain_calib[ch][bin].slope
                        + dom_calib->atwd0_gain_calib[ch][bin].y_intercept
                        - baseline[atwd][ch];
                }
                else {
                    v = (float)(channels[ch][bin]) * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept
                        - baseline[atwd][ch];
                }            
                /* Also subtract out bias voltage */
                v -= bias_v;
                charges[ch][trig] += v;

            } /* End integration loop */

        } /* End trigger loop */
    } /* End channel loop */
    
    /*---------------------------------------------------------------------------*/

    float mean, var;
    float freq = getCalibFreq(atwd, *dom_calib, AMP_CAL_SAMPLING_DAC);

    float gains[3][AMP_CAL_TRIG_CNT];
    for (ch = 0; ch < 3; ch++) {
        /* Pulser charge is golden reference! */
        float pulser_q = pulserDAC2Q(pulser_settings[ch]);        

        for (trig = 0; trig < AMP_CAL_TRIG_CNT; trig++)
            gains[ch][trig] = charges[ch][trig] / (freq * 50 * 1e-6 * pulser_q);

        /* Find gain and error */
        meanVarFloat(gains[ch], AMP_CAL_TRIG_CNT, &mean, &var);
        float err = sqrt(var / AMP_CAL_TRIG_CNT);

        /* Record in calibration struct */
        dom_calib->amplifier_calib[ch].value = mean;
        dom_calib->amplifier_calib[ch].error = err;

#ifdef DEBUG
        printf("ch %d gain (new): %.2f (err: %g)\r\n", ch, mean, err); 
#endif
    }

    /*---------------------------------------------------------------------------*/
    /* Put the DACs back to original state */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);

    /* Turn off the pulser */
    hal_FPGA_TEST_disable_pulser();

    /* FIX ME: return real error code */
    return 0;

}
