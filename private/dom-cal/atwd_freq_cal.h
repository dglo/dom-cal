/*
 * atwd_freq_cal header file
 */

/* How many pulser waveforms to measure */
#define ATWD_FREQ_CAL_TRIG_CNT    100

#define NUMBER_OF_SPEED_SETTINGS  6

/* Prototypes */
int atwd_freq_cal(calib_data *dom_calib);
