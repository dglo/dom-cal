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
    
    /* ATWD sampling speeds to be tested */
    const short speed_settings[NUMBER_OF_SPEED_SETTINGS] = 
                                  { 750, 1000, 1250, 1500, 1750, 2000 };
    
     /* Select oscillator analog mux input */
    halSelectAnalogMuxInput( DOM_HAL_MUX_OSC_OUTPUT );

    trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0;
    cal_loop( atwd0_cal, speed_settings,
                      trigger_mask, DOM_HAL_DAC_ATWD0_TRIGGER_BIAS );

    trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD1;
    cal_loop( atwd0_cal, speed_settings,
                      trigger_mask, DOM_HAL_DAC_ATWD1_TRIGGER_BIAS );

    linearFitFloat( speed_settings, atwd0_cal, NUMBER_OF_SPEED_SETTINGS,
                                             dom_calib->atwd0_freq_calib );

    linearFitFloat( speed_settings, atwd1_cal, NUMBER_OF_SPEED_SETTINGS,
                                             &dom_calib->atwd1_freq_calib );

    return 0;
}

void cal_loop( float *atwd_cal, short *speed_setings,
                             int trigger_mask, short ATWD_DAC_channel ) {
    
    int clock_waveform[128];
    float normalized_waveform[128];
    float bin_count[ATWD_FREQ_CAL_TRIG_CNT];

    prescanATWD( trigger_mask );
    
    for ( int i = 0; i < NUMBER_OF_SPEED_SETTINGS; i++ ) {
        
        halWriteDAC( ATWD_DAC_channel, speed_settings[i] );

        for ( int j = 0; j <  ATWD_FREQ_CAL_TRIG_CNT; j++ ) {

            int clock_waveform[128];
            hal_FPGA_TEST_trigger_forced( trigger_mask );
            hal_FPGA_TEST_readout( NULL, NULL, NULL, &clock_waveform,
                                               NULL, NULL, NULL, NULL,
                                               128, NULL, 0, trigger_mask );
            
            /* Remove DC */
            int sum = 0;
            for ( int k = 0; k < 128; k++ ) {
                sum += clock_waveform[k];
            }
            
            for ( int k = 0; k < 128; k++ ) {
                normalized_waveform[k] = ( float )clock / sum;
            }

            /* Calculate #bins between first and final
             * zero crossing with positive slope
             */
            int first_crossing = 0;
            int final_crossing = 0;
            int number_of_crossings = 0;
         
            for ( int k = 0; k < 127; k++ ) {
                if ( normalized_waveform[k] < 0
                           && normalized_waveform[k + 1] >= 0 ) {
                    if ( first_crossing == 0 ) {
                        first_crossing = k + 1;
                    } else {
                        final_crossing = k + 1;
                    }
                    number_of_crossings++;
                }
            }

            bin_count[j] =
                ( first_crossing - final_crossing ) / number_of_crossings;
        }
        
        /* Calculate average */
        float sum = 0.0;
        for ( int j = 0; j < ATWD_FREQ_CAL_TRIG_CNT; j++ ) {
            sum += bin_count[j];
        }
        atwd_cal[i] = sum / ATWD_FREQ_CAL_TRIG_CNT;
    }
}
