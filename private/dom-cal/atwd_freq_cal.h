/*
 * atwd_freq_cal header file
 */

/* How many pulser waveforms to measure */
#define ATWD_FREQ_CAL_TRIG_CNT    100

/* Number of TRIGGER_BIAS values to scan */
#define NUMBER_OF_SPEED_SETTINGS  6

/* Error code indicating a bad clock waveform was encountered */
#define UNUSABLE_CLOCK_WAVEFORM   2

/* Prototypes */
int atwd_freq_cal(calib_data *dom_calib);

int cal_loop( float *atwd_cal, short *speed_settings,
                             int trigger_mask, short ATWD_DAC_channel );
