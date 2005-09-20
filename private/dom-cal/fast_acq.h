/*
 * fast-acq.h -- utility for fast (no floating-point) DOM voltage waveform
 *               acquisition
 *
 */

#ifndef _FAST_ACQ_
#define _FAST_ACQ_

#include "domcal.h"

/* Basic integer volt measure */
/* volt_t * 10000000 = 1V -- volt_t is 10nV */
/* This allows -20V - +20V within 32bit range */ 
typedef int volt_t;

/* Integer representation of a linear fit */
/* Reasonable when m, b >> 1 */
typedef struct {

    volt_t slope, y_intercept;

} volt_t_fit;    

/* Calibration struct with integer values */
typedef struct {

    /* ATWD gain calibration */
    volt_t_fit atwd0_gain_calib[3][128];
    volt_t_fit atwd1_gain_calib[3][128];

    /* Copy of amplifier calibration */
    value_error amplifier_calib[3];

} integer_calib;

/* Unit conversions */
int to_volt_t(float v);
float to_v(volt_t v);

/* indicates voltage value is out of volt_t range */
#define OUT_OF_RANGE 0x7FFFFFFF;

/* Routine to build necessary integer calibration tables */
void build_integer_calib(calib_data *dom_calib, integer_calib *int_calib);

/* Routine to acquire calibrated waveform
 * cpu  ==> trig = 0;
 * LED  ==> trig = 1;
 * disc ==> trig = 2;
 *
 */

#define LED_TRIGGER  1
#define CPU_TRIGGER  0
#define DISC_TRIGGER 2

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
                 int prescan);
#endif
