/*
 * atwd_cal
 *
 * ATWD calibration routine -- records average pedestals for various
 * FE bias voltages, and then fits the relationship for each sample.  
 * Iterates over ATWD and channel as well.
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "atwd_cal.h"
#include "calUtils.h"

/*---------------------------------------------------------------------------*/

/* FIX ME -- use real data structure */
int atwd_cal(void) {

    const int cnt = 128;
    int trigger_mask, bias_idx, bias;
    int atwd, ch, bin, trig;
    float biases[BIAS_CNT];
    
    /* Channel readout buffers for each ATWD, channel, and bin */
    short channels[2][3][128];

    /* 
     * Allocate pedestal waveform buffers for each ATWD, 
     * channel, bin, and bias level:
     */
    float atwd_pedestal[2][3][128][BIAS_CNT];

    printf("Performing ATWD calibration...\r\n");

    /* Turn off clamping amplifier */
    /* FIX ME -- DOES THIS EVEN WORK? */    
    halWriteDAC(DOM_HAL_DAC_FE_AMP_LOWER_CLAMP, 0);

    /* Trigger both ATWDs */
    trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0 | HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Loop over several FE bias levels */
    bias_idx = 0;
    for (bias = BIAS_LOW; bias <= BIAS_HIGH; bias += BIAS_STEP) {

        printf(" setting FE bias DAC to %d\r\n", bias);

        /* Record the bias level, in volts, for the fit */
        biases[bias_idx] = biasDAC2V(bias);

        /* Set the FE bias level */
        halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, bias);
        halUSleep(250000);
        
        /* Initialize the pedestal array */
        for(atwd=0; atwd<2; atwd++)
            for(ch=0; ch<3; ch++)
                for(bin=0; bin<cnt; bin++)
                    atwd_pedestal[atwd][ch][bin][bias_idx] = 0.0;
        
        /* Warm up the ATWD */
        prescanATWD(trigger_mask);
        
        for (trig=0; trig<(int)ATWD_CAL_TRIG_CNT; trig++) {
            
            /* CPU-trigger the ATWD */
            hal_FPGA_TEST_trigger_forced(trigger_mask);
            
            /* Read out one waveform for all channels except 3 */        
            hal_FPGA_TEST_readout(channels[0][0], channels[0][1], channels[0][2], NULL, 
                                  channels[1][0], channels[1][1], channels[1][2], NULL,
                                  cnt, NULL, 0, trigger_mask);
            
            /* Sum the waveform */
            for(atwd=0; atwd<2; atwd++)
                for(ch=0; ch<3; ch++)
                    for(bin=0; bin<cnt; bin++)
                            atwd_pedestal[atwd][ch][bin][bias_idx] += channels[atwd][ch][bin];  

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
    float m, b, r2;

    for(atwd=0; atwd<2; atwd++) {
        for(ch=0; ch<3; ch++) {
            for(bin=0; bin<cnt; bin++) {
                linearFitFloat(atwd_pedestal[atwd][ch][bin], biases, BIAS_CNT, &m, &b, &r2); 
#ifdef DEBUG
                printf("ATWD: %d Ch: %d Bin: %d Slope: %.6g Int: %.6g R^2: %.6f\r\n",
                       atwd, ch, bin, m, b, r2);
#endif
            }
        }
    }
                
    /* FIX ME: turn clamping back on? */
    /* FIX ME: put results into structure */    
      
    return 0;

}
