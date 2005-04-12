/*
 * hv_amp_cal header file
 */

/* HV setting for calibration MUST be an exact voltage used in hv_baseline calibration*/
#define HV_AMP_CAL_VOLTS 1400

/* Disc setting to eliminate most SPE */
#define HV_AMP_CAL_DISC_DAC  1000

/* Max and min atwd bin cnt of ch i to calibrate ch i+1 */
#define HV_AMP_CAL_MIN_PULSE 600
#define HV_AMP_CAL_MAX_PULSE 800

/* Time constant for rate measurement */
#define TEST_TRIG_TIME 15 //seconds

/* LED initial amplitude -- used if not enough muons are present */
#define INIT_LED_AMPLITUDE 300

/* LED amplitude increment */
#define LED_AMPLITUDE_INC 50 

/* Minimum acceptable rate of pulses capable of amp calibration */
#define MIN_PULSE_RATE 0.5

/* Maximum amplitude of LED */
#define LED_MAX_AMPLITUDE 1023

/* Error code returned due to low rate */
#define ERR_LOW_RATE 2

/* Prototypes */
int hv_amp_cal(calib_data *dom_calib);
float measure_rate(int atwd, int ch);
