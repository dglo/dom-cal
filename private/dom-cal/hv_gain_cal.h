/*
 * hv_gain_cal header file
 */

/* Maximum allowed temperature to run calibration, in K */
#define GAIN_CAL_MAX_TEMP       250

/* Specific DAC values for this test */
#define GAIN_CAL_DISC_DAC       505

/* How many SPE waveforms to histogram */
#define GAIN_CAL_TRIG_CNT      1000

/* HV settings for gain calibration (in V) */
/* Starting value, amount to increment, and number of settings */
#define GAIN_CAL_HV_LOW        1200
#define GAIN_CAL_HV_INC         100
#define GAIN_CAL_HV_CNT           8

/* Histogram info */
#define GAIN_CAL_BINS           100

/* Largest P/V we might consider a real measurement */
/* Larger ones are not used in fit */
#define GAIN_CAL_MAX_SANE_PV    4.0

/* Charge of e, Coulombs */
#define Q_E               1.602E-19

/* Prototypes */
int hv_gain_cal(calib_data *dom_calib);
