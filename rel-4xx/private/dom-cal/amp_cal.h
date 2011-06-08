/*
 * amp_cal header file
 */

/* Which ATWD to use */
#ifdef DOMCAL_REV4
#define AMP_CAL_ATWD              1
#else
#define AMP_CAL_ATWD              0
#endif

/* Specific DAC values for this test */
#ifdef DOMCAL_REV5
#define AMP_CAL_PEDESTAL_DAC   2130
#define AMP_CAL_DISC_DAC        700
#define AMP_CAL_SAMPLING_DAC   2000 /* Note higher sampling speed */
#else
#define AMP_CAL_PEDESTAL_DAC   1925
#define AMP_CAL_DISC_DAC        650
#define AMP_CAL_SAMPLING_DAC   2000
#endif

/* How many pulser waveforms to measure */
#define AMP_CAL_TRIG_CNT        250

/* Pulser amplitude settings for each channel */
#define AMP_CAL_PULSER_AMP_0    450
#define AMP_CAL_PULSER_AMP_1   1000
#define AMP_CAL_PULSER_AMP_2   1000

/* What bin to start looking for pulser peak */
#define AMP_CAL_START_BIN        80

/* ATWD Integration window on either side of peak */
/* MIN bins *after* peak in time, MAX bins before peak */
/* related to sampling speed DAC */
/* Note long tail! */
#define AMP_CAL_INT_WIN_MIN  40
#define AMP_CAL_INT_WIN_MAX  6

/* Prototypes */
int amp_cal(calib_data *dom_calib);
