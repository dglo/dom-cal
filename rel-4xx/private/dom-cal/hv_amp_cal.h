/*
 * hv_amp_cal header file
 */

/* How many LED waveforms to measure */
#define HV_AMP_CAL_TRIG_CNT        250

/* HV setting for calibration MUST be an exact voltage used in hv_baseline calibration*/
#define HV_AMP_CAL_VOLTS 1400

/* Disc setting to eliminate most SPE */
#define HV_AMP_CAL_DISC_DAC  900

/* Max and min atwd peak cnt of ch i to calibrate ch i+1 */
#define HV_AMP_CAL_MIN_PULSE  600
#define HV_AMP_CAL_MAX_PULSE  800

/* Best peak cnt for amplifier calibration */
#define HV_AMP_CAL_BEST_PULSE 700

/* Time constant for rate measurement, seconds */
#define TEST_TRIG_TIME 5

/* Maximum and minimum amplitude of LED */
#define LED_MAX_AMPLITUDE 1023
#define LED_MIN_AMPLITUDE 0

/* Using LED -- use delay to keep pulse on 1st half of ATWD */
#define HV_AMP_CAL_START_BIN 0

/* Error code returned due to timeout */
#define ERR_TIMEOUT 3

/* Timeout interval for an iteration -- 6 minutes */
#define MAX_AMP_CAL_ITER_MS 360000

/* Charge integration limits, in ns */
#define CHARGE_REV_NS   20
#define CHARGE_FOW_NS   50 

/* ATWD launch delay */
#define HV_AMP_CAL_ATWD_LAUNCH_DELAY 6

/* Sampling speed */
#define HV_AMP_CAL_SAMPLING_DAC 1700

/* Histogram Settings */
#define HV_AMP_CAL_HISTOGRAM_DEC 5
#define HV_AMP_CAL_HISTOGRAM_TRIG_TIME 2

/* Prototypes */
int hv_amp_cal(calib_data *dom_calib);
float get_average_amplitude(int atwd, int ch, int sec);
