/*
 * fast-acq.c -- routines to acquire ATWD voltage data with greatly reduced
 *               floating point calls
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "fast_acq.h"
#include "calUtils.h"

/* Convert voltage v (in volts) to volt_t units */
volt_t to_volt_t(float v) {
    if (fabs(v) > 19.0) return OUT_OF_RANGE;
    return (volt_t)(v * 10000000);
}

/* Convert volt_t value back to volts */
float to_v(volt_t v) {
    return ((float)v)/10000000.0;
}

/* Build integer linear fit table for ATWD calibration
 * We could make the amplifier calibration integer as well
 * but this would be sloppy....just stick to the basics for now
 */
void build_integer_calib(calib_data *dom_calib, integer_calib *int_calib) {

    int ch, bin;

    for (ch = 0; ch < 3; ch++) {
        for (bin = 0; bin < 128; bin++) {
            /* Converting y-axis -- just use to_volt_t ! */
            int_calib->atwd0_gain_calib[ch][bin].slope =
                to_volt_t(dom_calib->atwd0_gain_calib[ch][bin].slope);
            int_calib->atwd0_gain_calib[ch][bin].y_intercept =
                to_volt_t(dom_calib->atwd0_gain_calib[ch][bin].y_intercept);

            int_calib->atwd1_gain_calib[ch][bin].slope =
                to_volt_t(dom_calib->atwd1_gain_calib[ch][bin].slope);
            int_calib->atwd1_gain_calib[ch][bin].y_intercept =
                to_volt_t(dom_calib->atwd1_gain_calib[ch][bin].y_intercept);

        }
        /* copy over amplifier data reference */
        int_calib->amplifier_calib[ch] = dom_calib->amplifier_calib[ch];
        
    }

}

/* 
 * Fast waveform acquisition call -- use integer calibration
 * information to eliminate wasteful floating point calls
 * Expect a factor ~5 improvement over fpu calibration when
 * amplifier calibration is requested, and ~100 factor when 
 * amplifier calibration is avoided!
 */

void fast_acq_wf(volt_t *wf_dat,
                 int atwd,
                 int cnt,
                 int offset,
                 int trigger_mask,
                 volt_t bias_v,
                 integer_calib *int_calib,
                 volt_t baseline[2][3],
                 int *ch,
                 int trig,
                 int do_amp_cal,
                 int prescan) {
    
    short channels[2][cnt];
    int bin;

    if (prescan) {

        /* Warm up the ATWD */
        prescanATWD(trigger_mask);

    }

    /* Which trigger for the ATWD? */
    if (trig == LED_TRIGGER) {
        hal_FPGA_TEST_trigger_LED(trigger_mask);
    } else if (trig == DISC_TRIGGER){
        hal_FPGA_TEST_trigger_disc(trigger_mask);
    } else {
        /* Default to CPU trigger */
        hal_FPGA_TEST_trigger_forced(trigger_mask);
    }

    /* Wait for done */
    while (!hal_FPGA_TEST_readout_done(trigger_mask));

    /* Read out only waveform from channels 0 and 1 */
    /* We need channel 1 if our PMT is exceptionally high gain! */
    if (atwd == 0) {
        hal_FPGA_TEST_readout(channels[0], channels[1], NULL, NULL,
                              NULL, NULL, NULL, NULL,
                              cnt, NULL, 0, trigger_mask);
    } else {
        hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                              channels[0], channels[1], NULL, NULL,
                              cnt, NULL, 0, trigger_mask);
    }

    /* Make sure we aren't in danger of saturating channel 0 */
    /* If so, switch to channel 1 */
    *ch = 0;
    for (bin = offset; bin < cnt; bin++) {
       if (channels[0][bin] > 800) {
            *ch = 1;
            break;
        }
    }

    /* Need to convert wf to volt_t output */
    for (bin = offset; bin < cnt; bin++) {

        if (atwd == 0) {
            wf_dat[bin] = channels[*ch][bin] * int_calib->atwd0_gain_calib[*ch][bin].slope
                + int_calib->atwd0_gain_calib[*ch][bin].y_intercept
                - baseline[atwd][*ch];
        } else {
            wf_dat[bin] = channels[*ch][bin] * int_calib->atwd1_gain_calib[*ch][bin].slope
                + int_calib->atwd1_gain_calib[*ch][bin].y_intercept
                - baseline[atwd][*ch];
        }

        /* Also subtract out bias voltage */
        wf_dat[bin] -= bias_v;

        /* Divide by measured amplifier gain if requested */
        if (do_amp_cal) {
            wf_dat[bin] = (volt_t)((float)wf_dat[bin] / int_calib->amplifier_calib[*ch].value);
        }

    }
}
