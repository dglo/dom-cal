/*
 * amp_cal header file
 */

/* Which ATWD to use */
#define AMP_CAL_ATWD              1

/* Specific DAC values for this test */
#ifdef DOMCAL_REV5
#define AMP_CAL_PEDESTAL_DAC   2130
#define AMP_CAL_DISC_DAC        700
#define AMP_CAL_SAMPLING_DAC   2047
#else
#define AMP_CAL_PEDESTAL_DAC   1925
#define AMP_CAL_DISC_DAC        650
#define AMP_CAL_SAMPLING_DAC   2047
#endif
/* How many pulser waveforms to measure */
#define AMP_CAL_TRIG_CNT        250

/* Pulser amplitude settings for each channel */
#define AMP_CAL_PULSER_AMP_0    300
#define AMP_CAL_PULSER_AMP_1    600
#define AMP_CAL_PULSER_AMP_2   1000

/* Prototypes */
int amp_cal(calib_data *dom_calib);
