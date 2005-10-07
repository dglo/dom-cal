/*
 * atwd_freq_cal header file
 */

/* How many pulser waveforms to measure */
#define ATWD_FREQ_CAL_TRIG_CNT    100

/* Number of TRIGGER_BIAS values to scan */
#define NUMBER_OF_SPEED_SETTINGS  8

/* Error code indicating a bad clock waveform was encountered */
#define UNUSABLE_CLOCK_WAVEFORM   2

/* Maximum permissible number of bad clk waveforms */
#define MAX_BAD_CNT   10

/* Prototypes */
int atwd_freq_cal(calib_data *dom_calib);
int atwd_get_frq(int trigger_mask, float *ratio);
