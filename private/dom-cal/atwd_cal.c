/*
 * atwd_cal
 *
 * ATWD calibration routine -- records average pedestals for various
 * FE bias voltages, and then fits the relationship for each sample.  
 * Iterates over ATWD and channel as well.
 *
 */

#include <stdio.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "atwd_cal.h"
#include "calUtils.h"

/*---------------------------------------------------------------------------*/
int atwd_cal(calib_data *dom_calib) {

    const int cnt = 128;
    int trigger_mask, bias_idx, bias;
    int atwd, ch, bin, trig;
    float biases[BIAS_CNT];
    
    /* Channel readout buffers for each channel, and bin */
    short channels[3][128];

    /* 
     * Allocate pedestal waveform buffers for each ATWD, 
     * channel, bin, and bias level:
     */
    float atwd_pedestal[2][3][128][BIAS_CNT];

#ifdef DEBUG
    printf("Performing ATWD calibration...\r\n");
#endif

    /* Save DACs that we modify */
    short origBiasDAC  = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origClampDAC = halReadDAC(DOM_HAL_DAC_FE_AMP_LOWER_CLAMP);

    /* Turn off clamping amplifier */
    halWriteDAC(DOM_HAL_DAC_FE_AMP_LOWER_CLAMP, 0);

    /* Loop over several FE bias levels */
    bias_idx = 0;
    for (bias = BIAS_LOW; bias <= BIAS_HIGH; bias += BIAS_STEP) {

        /* Record the bias level, in volts, for the fit */
        biases[bias_idx] = biasDAC2V(bias);

        /* Set the FE bias level */
        halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, bias);
        halUSleep(DAC_SET_WAIT);
        
        /* Initialize the pedestal array */
        for(atwd=0; atwd<2; atwd++)
            for(ch=0; ch<3; ch++)
                for(bin=0; bin<cnt; bin++)
                    atwd_pedestal[atwd][ch][bin][bias_idx] = 0.0;

        /* Do NOT record both ATWDs at the same time -- this can change the baseline */
        /* and affect the calibration */
        for (atwd=0; atwd<2; atwd++) {

            /* Trigger a single ATWD at a time */
            trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : 
                HAL_FPGA_TEST_TRIGGER_ATWD1;
        
            /* Warm up the ATWD */
            prescanATWD(trigger_mask);
        
            for (trig=0; trig<(int)ATWD_CAL_TRIG_CNT; trig++) {
            
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
            
                /* Sum the waveform */
                for(ch=0; ch<3; ch++)
                    for(bin=0; bin<cnt; bin++)
                        atwd_pedestal[atwd][ch][bin][bias_idx] += channels[ch][bin];  

            }
        }
        
        /* 
         * Divide the resulting sum waveform by count to get an average
         * waveform.
         */
        for(atwd=0; atwd<2; atwd++)
            for(ch=0; ch<3; ch++)
                for(bin=0; bin<cnt; bin++)
                    atwd_pedestal[atwd][ch][bin][bias_idx] /= (float)ATWD_CAL_TRIG_CNT;

        bias_idx++;
    }

    /* Fit each ATWD, channel, and bin */
    for(ch=0; ch<3; ch++) {
        for(bin=0; bin<cnt; bin++) {
            linearFitFloat(atwd_pedestal[0][ch][bin], biases, BIAS_CNT, 
                           &(dom_calib->atwd0_gain_calib[ch][bin])); 
            linearFitFloat(atwd_pedestal[1][ch][bin], biases, BIAS_CNT, 
                           &(dom_calib->atwd1_gain_calib[ch][bin])); 
        }
    }

    /* Return DOM to previous state */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);
    halWriteDAC(DOM_HAL_DAC_FE_AMP_LOWER_CLAMP, origClampDAC);

    /* FIX ME: return real error code? */
    return 0;

}
