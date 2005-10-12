/*
 * amp_cal
 *
 * Amplifier calibration routine -- record waveform peaks in 
 * ATWD channels for a given pulser amplitude, and use pulser
 * and ATWD calibration to find gain of each amplifier.  Uses
 * only ATWD A (ATWD0).
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "amp_cal.h"
#include "calUtils.h"

/*---------------------------------------------------------------------------*/
/* FIX ME -- use real data structure */

int amp_cal(void) {

    const int cnt = 128;
    int trigger_mask;
    int ch, bin, trig;
    float bias_v, peak_v;
    
    /* Pulser amplitude settings for each channel */
    /* const int pulser_settings[3] = {50, 200, 1000}; */
    const int pulser_settings[3] = {400, 800, 1000};
    
    /* Channel readout buffers for each channel and bin */
    /* This test only uses ATWD A */
    short channels[3][128];

    /* Peak arrays for each channel, in Volts */
    float peaks[3][AMP_CAL_TRIG_CNT];

    /* Initialize the peak array */
    for (ch = 0; ch < 3; ch++) 
        for (trig = 0; trig < AMP_CAL_TRIG_CNT; trig++)
            peaks[ch][trig] = 0.0;

    printf("Performing amplifier calibration...\r\n");

    /* Set discriminator and bias level */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, AMP_CAL_PEDESTAL_DAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, AMP_CAL_DISC_DAC);

    bias_v = biasDAC2V(AMP_CAL_PEDESTAL_DAC);

    /* Turn on the pulser */
    hal_FPGA_TEST_enable_pulser();

    /* Trigger ATWD A only */
    trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0;

    /* Loop over channels and pulser settings for each channel */
    for (ch = 0; ch < 3; ch++) {

        printf(" measuring amplification of channel %d\r\n", ch);

        /* Set the pulser amplitude */
        printf("  setting pulser to %d\r\n", pulser_settings[ch]);
        halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, pulser_settings[ch]);

        /* Wait just a bit */
        halUSleep(250000);
                
        /* Warm up the ATWD */
        prescanATWD(trigger_mask);
        
        for (trig=0; trig<(int)AMP_CAL_TRIG_CNT; trig++) {
            
            /* Discriminator trigger the ATWD */
            hal_FPGA_TEST_trigger_disc(trigger_mask);
            
            /* Read out one waveform for all channels except 3 */        
            hal_FPGA_TEST_readout(channels[0], channels[1], channels[2], NULL, 
                                  NULL, NULL, NULL, NULL,
                                  cnt, NULL, 0, trigger_mask);
            
            /* Find and record the peak for the channel we're calibrating */
            for (bin=0; bin<cnt; bin++) {

                /* Using ATWD calibration data, convert to actual V */
                /* FIX ME */
                peak_v = (float)(channels[ch][bin]) * -1.0;

                /* Note "peak" is actually a minimum */
                peaks[ch][trig] = (peak_v < peaks[ch][trig]) ? 
                    peak_v : peaks[ch][trig];
            }

            printf("Trig %d peak %.4f\r\n", trig, peaks[ch][trig]);
        }
    }

    /* FIX ME: Pulser amplitude in volts */
    float pulser_v = 1.0;

    /* FIX ME: Find mean and error */
    float mean, var, gain, error;
    for (ch = 0; ch < 3; ch++) {
        meanVarFloat(peaks[ch], AMP_CAL_TRIG_CNT, &mean, &var);
        gain = mean / pulser_v;
        error = sqrt(var)/(pulser_v*sqrt(AMP_CAL_TRIG_CNT));

#ifdef DEBUG
        printf(" Channel %d: gain %.6g, error %.6g\r\n", ch, gain, error);
#endif
    }

    /* Turn off the pulser */
    hal_FPGA_TEST_disable_pulser();

    /* FIX ME: put results into structure */    
      
    return 0;

}
