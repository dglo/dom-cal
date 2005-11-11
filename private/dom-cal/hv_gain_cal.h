/*
 * hv_gain_cal header file
 */

/* Specific DAC values for this test */
#ifdef DOMCAL_REV5
#define GAIN_CAL_DISC_DAC_LOW   550
#define GAIN_CAL_DISC_DAC_MED   556
#define GAIN_CAL_DISC_DAC_HIGH  600
#else
#define GAIN_CAL_DISC_DAC_LOW   499
#define GAIN_CAL_DISC_DAC_MED   505
#define GAIN_CAL_DISC_DAC_HIGH  449
#endif

/* Which ATWD to use */
#ifdef DOMCAL_REV4
#define GAIN_CAL_ATWD             1
#else
#define GAIN_CAL_ATWD             0
#endif

/* How many SPE waveforms to histogram */
#define GAIN_CAL_TRIG_CNT     25000

/* HV settings for gain calibration (in V) */
/* Starting value, amount to increment, and number of settings */
#define GAIN_CAL_HV_LOW        1020
#define GAIN_CAL_HV_INC          80
#define GAIN_CAL_HV_CNT          12

/* Histogram info */
#define GAIN_CAL_BINS           250

/* Integration window on either side of peak */
/* 8 bins *after* peak in time, 4 bins before peak */
#define INT_WIN_MIN               8
#define INT_WIN_MAX               4

/* Charge of e, Coulombs */
#define Q_E               1.602E-19

/* Number of noise readings to take before determining if noise level is sane */
#define NOISE_CNT                10

/* Minimum acceptable noise level to fill histogram */
#define MIN_NOISE                400

/* Number of PMT baseline ATWD readouts to take */
#define BASELINE_TRIG_CNT      10

/* bin to start looking for disc pulse */
#define GAIN_CAL_START_BIN     96

/* Prototypes */
int hv_gain_cal(calib_data *dom_calib);
