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

/*---------------------------------------------------------------------------*/
int amp_cal(calib_data *dom_calib) {

    const int cnt = 128;
    int trigger_mask;
    int ch, bin, trig;
    float bias_v, peak_v;
    
    /* Which atwd to use */
    short atwd = AMP_CAL_ATWD;

    /* Pulser amplitude settings for each channel */
    const int pulser_settings[3] = {AMP_CAL_PULSER_AMP_0, 
                                    AMP_CAL_PULSER_AMP_1, 
                                    AMP_CAL_PULSER_AMP_2 };

    /* Channel readout buffers for each channel and bin */
    /* This test only uses one ATWD */
    short channels[3][128];

    /* Peak arrays for each channel, in Volts */
    float peaks[3][AMP_CAL_TRIG_CNT];

#ifdef DEBUG
    printf("Performing amplifier calibration...\r\n");
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

    /* Turn on the pulser */
    hal_FPGA_TEST_enable_pulser();

    /* Trigger one ATWD only */
    trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Loop over channels and pulser settings for each channel */
    for (ch = 0; ch < 3; ch++) {

        /* Set the pulser amplitude */
        halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, pulser_settings[ch]);

        /* Wait just a bit */
        halUSleep(250000);
                
        /* Warm up the ATWD */
        prescanATWD(trigger_mask);
        
        for (trig=0; trig<(int)AMP_CAL_TRIG_CNT; trig++) {
            
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
            for (bin=0; bin<cnt; bin++) {

                /* Using ATWD calibration data, convert to actual V */
                if (atwd == 0) {
                    peak_v = (float)(channels[ch][bin]) * dom_calib->atwd0_gain_calib[ch][bin].slope
                        + dom_calib->atwd0_gain_calib[ch][bin].y_intercept;
                }
                else {
                    peak_v = (float)(channels[ch][bin]) * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept;
                }

                /* Also subtract out bias voltage */
                peak_v -= bias_v;

                /* Note "peak" is actually a minimum */
                if (bin == 0) {
                    peaks[ch][trig] = peak_v;
                }
                else {
                    peaks[ch][trig] = (peak_v < peaks[ch][trig]) ? 
                        peak_v : peaks[ch][trig];
                }
            }
        }
    }


    float mean, var, pulser_v;
    for (ch = 0; ch < 3; ch++) {
        /* Use pulser calibration to convert pulser amplitude to V */
        pulser_v = (dom_calib->pulser_calib.slope * pulser_settings[ch]) + 
            dom_calib->pulser_calib.y_intercept;

        /* Find gain and error */
        meanVarFloat(peaks[ch], AMP_CAL_TRIG_CNT, &mean, &var);
        dom_calib->amplifier_calib[ch].value = mean / pulser_v;
        dom_calib->amplifier_calib[ch].error =  sqrt(var)/(pulser_v*sqrt(AMP_CAL_TRIG_CNT));

    }

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
