/*
 * atwd_freq_cal
 *
 * ATWD frequency calibration routine -- Acquire clock
 * oscillator waveform and calculate #bins/clock cycle
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "atwd_freq_cal.h"
#include "calUtils.h"

/*---------------------------------------------------------------------------*/
int atwd_freq_cal(calib_data *dom_calib) {

    int trigger_mask;
    float atwd0_cal[NUMBER_OF_SPEED_SETTINGS];
    float atwd1_cal[NUMBER_OF_SPEED_SETTINGS];

#ifdef DEBUG    
    printf( "Performing ATWD frequency calibration...\r\n" );
#endif

    /* Record current DOM state */
    int old_ATWD0_bias = halReadDAC( DOM_HAL_DAC_ATWD0_TRIGGER_BIAS );
    int old_ATWD1_bias = halReadDAC( DOM_HAL_DAC_ATWD1_TRIGGER_BIAS );

    /* ATWD sampling speeds to be tested */
    short speed_settings[NUMBER_OF_SPEED_SETTINGS] = 
                                  { 750, 1000, 1250, 1500, 1750, 2000 };
    
    /* Select oscillator analog mux input */
    halSelectAnalogMuxInput( DOM_HAL_MUX_OSC_OUTPUT );

    /* Calibrate ATWD0 */
    trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0;
    int ret0 = cal_loop( atwd0_cal, speed_settings,
                      trigger_mask, DOM_HAL_DAC_ATWD0_TRIGGER_BIAS );
    
    /* Calibrate ATWD1 */
    trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD1;
    int ret1 = cal_loop( atwd1_cal, speed_settings,
                      trigger_mask, DOM_HAL_DAC_ATWD1_TRIGGER_BIAS );

    /* Store speed as float array needed by linearFitFloat */
    float speed_settingsf[NUMBER_OF_SPEED_SETTINGS];
    int i;
    for ( i = 0; i < NUMBER_OF_SPEED_SETTINGS; i++ ) {
        speed_settingsf[i] = speed_settings[i];
    }

    /* Fit and store ATWD0 calibration */
    linearFitFloat( speed_settingsf, atwd0_cal, NUMBER_OF_SPEED_SETTINGS,
                                             &dom_calib->atwd0_freq_calib );
    
    /* Fit and store ATWD1 calibration */
    linearFitFloat( speed_settingsf, atwd1_cal, NUMBER_OF_SPEED_SETTINGS,
                                             &dom_calib->atwd1_freq_calib );

    /* Restore DOM state */
    halWriteDAC( DOM_HAL_DAC_ATWD0_TRIGGER_BIAS, old_ATWD0_bias );
    halWriteDAC( DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, old_ATWD1_bias );

    if ( ret0 != 0 ) {
        return ret0;
    } else {
        return ret1;
    }
}

int cal_loop( float *atwd_cal, short *speed_settings,
                             int trigger_mask, short ATWD_DAC_channel ) {
    
    /* Raw ATWD ch3 waveform from oscillator */
    short clock_waveform[128];
    
    /* Normalized oscillator waveform */
    float normalized_waveform[128];

    /* Store ATWD_FREQ_CAL_TRIG_CNT frequency ratio calculations */
    float bin_count[ATWD_FREQ_CAL_TRIG_CNT];

    /* Get rid of first several garbage ATWD shots */
    prescanATWD( trigger_mask );
    
    int i;
    int j;
    int k;
 
    for ( i = 0; i < NUMBER_OF_SPEED_SETTINGS; i++ ) {
        
        /* Set speed DAC */
        halWriteDAC( ATWD_DAC_channel, speed_settings[i] );
        
        /* Take  ATWD_FREQ_CAL_TRIG_CNT waveforms */
        for ( j = 0; j <  ATWD_FREQ_CAL_TRIG_CNT; j++ ) {

            /* Force ATWD readout */
            hal_FPGA_TEST_trigger_forced( trigger_mask );
            
            /* Read ATWD ch3 waveform */
            if ( trigger_mask == HAL_FPGA_TEST_TRIGGER_ATWD0 ) {
                hal_FPGA_TEST_readout( NULL, NULL, NULL, clock_waveform,
                                               NULL, NULL, NULL, NULL,
                                               128, NULL, 0, trigger_mask );
            } else {
                hal_FPGA_TEST_readout( NULL, NULL, NULL, NULL,
                                             NULL, NULL, NULL, clock_waveform,
                                             128, NULL, 0, trigger_mask );
            }
            
            /* Remove DC */
            int sum = 0;
            for ( k = 0; k < 128; k++ ) {
                sum += clock_waveform[k];
            }
            float average = ( float )sum / 128;
            for ( k = 0; k < 128; k++ ) {
                normalized_waveform[k] = ( float )clock_waveform[k] - average;
            }

            /* Calculate #bins between first and final
             * zero crossing with positive slope
             */
            int first_crossing = 0;
            int final_crossing = 0;
            int number_of_cycles = 0;
         
            for ( k = 0; k < 127; k++ ) {
                
                /* Look for zero crossings with positive slope */
                if ( normalized_waveform[k] < 0
                           && !( normalized_waveform[k + 1] < 0 ) ) {
                    if ( first_crossing == 0 ) {
                        first_crossing = k + 1;
                    } else {
                        final_crossing = k + 1;
                        number_of_cycles++;
                    }
                }
            }

            /* Need at least one cycle to analyze */
            if ( number_of_cycles < 1 ) {
                return UNUSABLE_CLOCK_WAVEFORM;
            }

            /* Calculate average number of bins per clock cycle --
             * this is the clock ratio
             */
            int bins = final_crossing - first_crossing;
            bin_count[j] = ( float )bins / number_of_cycles;
        }
        
        /* Calculate average */
        float sum = 0.0;
        for ( j = 0; j < ATWD_FREQ_CAL_TRIG_CNT; j++ ) {
            sum += bin_count[j];
        }
        
        /* Store final average value */
        atwd_cal[i] = sum / ATWD_FREQ_CAL_TRIG_CNT;
    }
    return 0;
}
