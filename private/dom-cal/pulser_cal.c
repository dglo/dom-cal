/*
 * pulser_cal
 *
 * Analog FE pulser calibration routine -- Determine pulser amplitude
 * relationship to DAC setting using SPE discriminator
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "pulser_cal.h"
#include "calUtils.h"

/*---------------------------------------------------------------------------*/
int pulser_cal(calib_data *dom_calib) {
    
    float pulser_cal_data[NUMBER_OF_SPE_SETTINGS];
    
    /* ATWD sampling speeds to be tested */
    const short spe_settings[NUMBER_OF_SPE_SETTINGS] = 
             { 525, 550, 575, 600, 625, 650, 675, 700, 750, 800, 900, 1000 };
    
     /* Select oscillator analog mux input */
    halWriteDAC( DOM_HAL_DAC_PMT_FE_PEDESTAL, PEDESTAL_VALUE );
    
    /* Turn pulser on */
    hal_FPGA_TEST_enable_pulser();

    for ( int i = 0; i < NUMBER_OF_SPE_SETTINGS; i++ ) {
        
        /* Set SPE threshold */
        halWriteDAC( DOM_HAL_DAC_SINGLE_SPE_THRESHOLD, spe_settings[i] );

        /* Find pulser amplitude corresponding to SPE threshold */
        int max = 1000;
        int min = 0;
        int amplitude;
        while ( max - min > 1 ) {
            amplitude = ( max + min ) / 2;
            
            /* Set pulser amplitude and wait */
            halWriteDAC( DOM_HAL_DAC_INTERNAL_PULSER, amplitude );
            halUSleep(250000);

            int spe_rate = hal_FPGA_TEST_get_spe_rate();
            if ( spe_rate < 3900 ) {
                min = amplitude;
            } else {
                max = amplitude;
            }
        }

        pulser_cal_data[i] = amplitude;
    }

    float volt_spe_settings[NUMBER_OF_SPE_SETTINGS];
    for ( int i = 0; i < NUMBER_OF_SPE_SETTINGS; i++ ) {
        volt_spe_settings[i] = biasDAC2V( spe_settings[i] );
    }

    linearFitFloat( pulser_cal_data, volt_spe_settings,
                  NUMBER_OF_SPE_SETTINGS, &dom_calib->pulser_calib );

    /* Turn pulser off */
    hal_FPGA_TEST_disable_pulser();
    
    return 0;
}
