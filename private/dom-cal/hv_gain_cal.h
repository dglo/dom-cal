/*
 * hv_gain_cal header file
 */

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
#define HIST_BIN_CNT            100

/* Prototypes */
int hv_gain_cal(calib_data *dom_calib);
