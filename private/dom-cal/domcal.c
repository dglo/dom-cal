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
#include "calUtils.h"

/* Include calibration module headers here */
#include "atwd_cal.h"
#include "amp_cal.h"

/*---------------------------------------------------------------------------*/

void init_dom(void) {

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
/* 
 * record_state
 *
 * Records the state of the DOM before data taking -- all DAC and ADC settings,
 * temperature, etc.
 *
 */
void record_state(calib_data *dom_calib) {

    int i;

    /* Read ID */
    strcpy(dom_calib->dom_id, halGetBoardID());

    /* Read DACs */
    for (i = 0; i < 16; i++)
        dom_calib->dac_values[i] = halReadDAC(i);

    /* Read ADCs */
    for (i = 0; i < 24; i++)
        dom_calib->adc_values[i] = halReadADC(i);

    /* Read temperature and convert to K */
    dom_calib->temp = temp2K(halReadTemp());
}

/*---------------------------------------------------------------------------*/
/*
 * save_results
 *
 * Save calibration results to the flash filesystem.
 *
 */

int save_results(calib_data dom_calib) {

    int i, ch, bin;
    int err = 0;

    /* FIX ME: For now, just print everything out */
#ifdef DEBUG
    printf("ID: %s\r\n", dom_calib.dom_id);

    for (i = 0; i < 16; i++)
        printf("DAC %d: %d\r\n", i, dom_calib.dac_values[i]);

    for (i = 0; i < 24; i++)
        printf("ADC %d: %d\r\n", i, dom_calib.adc_values[i]);

    printf("Temp: %.1f\r\n", dom_calib.temp);

    for(ch = 0; ch < 3; ch++)
        for(bin = 0; bin < 128; bin++)
            printf("ATWD0 Ch %d Bin %d Fit: m=%.6g b=%.6g r^2=%.6g\r\n",
                   ch, bin, dom_calib.atwd0_gain_calib[ch][bin].slope,
                   dom_calib.atwd0_gain_calib[ch][bin].y_intercept,
                   dom_calib.atwd0_gain_calib[ch][bin].r_squared);

    for(ch = 0; ch < 3; ch++)
        for(bin = 0; bin < 128; bin++)
            printf("ATWD1 Ch %d Bin %d Fit: m=%.6g b=%.6g r^2=%.6g\r\n",
                   ch, bin, dom_calib.atwd1_gain_calib[ch][bin].slope,
                   dom_calib.atwd1_gain_calib[ch][bin].y_intercept,
                   dom_calib.atwd1_gain_calib[ch][bin].r_squared);

    for(ch = 0; ch < 3; ch++)
        printf("Channel %d gain=%.6g error=%.6g\r\n", ch,
               dom_calib.amplifier_calib[ch].value,
               dom_calib.amplifier_calib[ch].error);

#endif

    return err;
}

/*---------------------------------------------------------------------------*/

int main(void) {
    
    int err = 0;
    calib_data dom_calib;
    
#ifdef DEBUG
    printf("Hello, world!  This is domcal version %d.%d\r\n", 
           MAJOR_VERSION, MINOR_VERSION);
#endif
    
    /* Initialize DOM state: DACs, HV setting, pulser, etc. */
    init_dom();
    
    /* Record DOM state, etc. */
    record_state(&dom_calib);
    
    /* FIX ME: get date, don't forget about version number */
    
    /* Calibration modules:
     *  - pulser calibration
     *  - atwd calibration
     *  - amplifier calibration
     *  - sampling speed calibration
     */
    /* FIX ME: return real error codes or something */
    atwd_cal(&dom_calib);
    amp_cal(&dom_calib);
    
    /* Write calibration record to flash */
    save_results(dom_calib);

#ifdef DEBUG
    if (!err) 
        printf("Calibration completed successfully.\r\n");
#endif
   
    /* Reboot the DOM -- DOESN'T SEEM TO WORK */
    hal_FPGA_TEST_request_reboot();
    while (!hal_FPGA_TEST_is_reboot_granted());

    return 0;
}
