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
    int old_pedestal_value;
    int old_spe_thresh;
    int old_pulser_amplitude;
    
    /* ATWD sampling speeds to be tested */
    const short spe_settings[NUMBER_OF_SPE_SETTINGS] = 
             { 525, 550, 575, 600, 625, 650, 675, 700, 750, 800, 900, 1000 };
    
    /* Record current DAC values */
    old_pedestal_value = halReadDAC( DOM_HAL_DAC_PMT_FE_PEDESTAL );
    old_spe_thresh = halReadDAC( DOM_HAL_DAC_SINGLE_SPE_THRESH );
    old_pulser_amplitude = halReadDAC( DOM_HAL_DAC_INTERNAL_PULSER );

     /* Set pedestal value */
    halWriteDAC( DOM_HAL_DAC_PMT_FE_PEDESTAL, PEDESTAL_VALUE );
    halUSleep( 100000 );
    int bias = halReadDAC( DOM_HAL_DAC_PMT_FE_PEDESTAL );
    
    /* Turn pulser on */
    hal_FPGA_TEST_enable_pulser();

    int i;
    for ( i = 0; i < NUMBER_OF_SPE_SETTINGS; i++ ) {
        
        /* Set SPE threshold */
        halWriteDAC( DOM_HAL_DAC_SINGLE_SPE_THRESH, spe_settings[i] );

        /* Find pulser amplitude corresponding to SPE threshold */
        int max = 1000;
        int min = 0;
        int amplitude;
        int count;

        /* Continue until max and min differ by one bin */
        for ( count = 0; max - min > 1; count++ ) {
            
            /* Should require 0(10) iterations. >25 implies an error occurred */
            if ( count > 25 ) {
                return AMPLITUDE_NOT_LOCALIZED;
            }

            amplitude = ( max + min ) / 2;
            
            /* Set pulser amplitude and wait */
            halWriteDAC( DOM_HAL_DAC_INTERNAL_PULSER, amplitude );
            halUSleep( 250000 );

            /* Retreive current spe rate */
            int spe_rate = hal_FPGA_TEST_get_spe_rate();
            
            /* Search for amplitude where half ( ~3900 ) pulses cross thresh */
            if ( spe_rate < 3900 ) {
                min = amplitude;
            } else {
                max = amplitude;
            }
        }

        pulser_cal_data[i] = amplitude;
    }

    /* Convert SPE value to volts */
    float volt_spe_settings[NUMBER_OF_SPE_SETTINGS];
    for ( i = 0; i < NUMBER_OF_SPE_SETTINGS; i++ ) {
        volt_spe_settings[i] = 
               0.0000244 * ( 0.4 * spe_settings[i] - 0.1 * bias ) * 5;
    }

    /* Do linear fit, x-axis pulser amplitude, y-axis SPE thresh */
    linearFitFloat( pulser_cal_data, volt_spe_settings,
                  NUMBER_OF_SPE_SETTINGS, &dom_calib->pulser_calib );

    /* Restore DAC values */
    halWriteDAC( DOM_HAL_DAC_PMT_FE_PEDESTAL, old_pedestal_value );
    halWriteDAC( DOM_HAL_DAC_SINGLE_SPE_THRESH, old_spe_thresh );
    halWriteDAC( DOM_HAL_DAC_INTERNAL_PULSER, old_pulser_amplitude );

    /* Turn pulser off */
    hal_FPGA_TEST_disable_pulser();
    
    return 0;
}
