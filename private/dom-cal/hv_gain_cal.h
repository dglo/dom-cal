/*
 * hv_gain_cal header file
 */

/* Discriminator charge thresholds */
#define GAIN_CAL_PC_LOW   0.12
#define GAIN_CAL_PC_MED   0.25
#define GAIN_CAL_PC_HIGH  1.0

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

/* In the case of multiple iterations of HV/gain calibration, */
/* iterate this many times */
#define GAIN_CAL_MULTI_ITER       4

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

/* Minimum acceptable R^2 */
#define GAIN_CAL_MIN_R2        0.99

/* Decrease points in fit until you have this many or fewer */
#define GAIN_CAL_MIN_R2_PTS    4

/* Prototypes */
int hv_gain_cal(calib_data *dom_calib, int iterHVGain);
