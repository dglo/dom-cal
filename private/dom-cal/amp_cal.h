/*
 * amp_cal header file
 */

/* Specific DAC values for this test */
#define AMP_CAL_PEDESTAL_DAC   1925
#define AMP_CAL_DISC_DAC        550

/* How many pulser waveforms to measure */
#define AMP_CAL_TRIG_CNT 250

/* Prototypes */
int amp_cal(calib_data *dom_calib);
