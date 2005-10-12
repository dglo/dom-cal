/*
 * domcal
 *
 * IceCube DOM front-end calibration application.
 * 
 * John Kelley and Jim Braun
 * UW-Madison, 2004
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"

/* Include calibration module headers here */
#include "atwd_cal.h"
#include "amp_cal.h"

/*---------------------------------------------------------------------------*/

/* This may go away? Should we use boot state of DOM? */
void initDOM(void) {

    int atwd;

    /* Make sure HV is off */
    /* halPowerDownBase(); */

    /* Set up DACs */
    halWriteDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS, ATWD_SAMPLING_SPEED_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD0_RAMP_TOP, ATWD_RAMP_TOP_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD0_RAMP_RATE, ATWD_RAMP_BIAS_DAC);

    halWriteDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, ATWD_SAMPLING_SPEED_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD1_RAMP_TOP, ATWD_RAMP_TOP_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD1_RAMP_RATE, ATWD_RAMP_BIAS_DAC);

    halWriteDAC(DOM_HAL_DAC_ATWD_ANALOG_REF, ATWD_ANALOG_REF_DAC);
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, ATWD_PEDESTAL_DAC);   

    /* Make sure pulser is off */
    hal_FPGA_TEST_disable_pulser();

}

/*---------------------------------------------------------------------------*/

int main(void) {

    int err = 0;
    int i;

#ifdef DEBUG
    printf("Hello, world!  This is domcal version %d.%d\r\n", 
           MAJOR_VERSION, MINOR_VERSION);
#endif
    
    /* Possible flow: */

    /* Initialize DOM state? DACs, HV setting, pulser, etc. */
    initDOM();

    /* Record DOM state, etc. (separate function): 
     *  - version of calibration program
     *  - DOM ID
     *  - Temperature
     *  - DACs and ADCs
     *  - anything else?  ask user to put in date?
     */
    
    /* Calibration modules:
     *  - pulser calibration
     *  - atwd calibration
     *  - amplifier calibration
     *  - sampling speed calibration
     */

    /* FIX ME: record results, return error code */
    atwd_cal();
    amp_cal();

    /* Write calibration record to flash (separate function) */

#ifdef DEBUG
    if (!err) 
        printf("Calibration completed successfully.\r\n");
#endif
   
    return err;
}
