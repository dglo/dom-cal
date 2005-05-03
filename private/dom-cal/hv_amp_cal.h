/*
 * hv_amp_cal header file
 */

/* HV setting for calibration MUST be an exact voltage used in hv_baseline calibration*/
#define HV_AMP_CAL_VOLTS 1200

/* Disc setting to eliminate most SPE */
#define HV_AMP_CAL_DISC_DAC  1000

/* Max and min atwd bin cnt of ch i to calibrate ch i+1 */
#define HV_AMP_CAL_MIN_PULSE 600
#define HV_AMP_CAL_MAX_PULSE 800

/* Time constant for rate measurement */
#define TEST_TRIG_TIME 15 //seconds

/* LED initial amplitude -- used if not enough muons are present */
#define INIT_LED_AMPLITUDE 700

/* LED amplitude decrement */
#define LED_AMPLITUDE_DEC 25 

/* Minimum acceptable rate of pulses capable of amp calibration */
#define MIN_PULSE_RATE 30.0

/* Maximum amplitude of LED */
#define LED_MAX_AMPLITUDE 1023

/* Error code returned due to low rate */
#define ERR_LOW_RATE 2

/* Charge integration limits */
#define CHARGE_REV_BINS 6
#define CHARGE_FOW_BINS 12

/* Prototypes */
int hv_amp_cal(calib_data *dom_calib);
float measure_rate(int atwd, int ch);
short get_led_amp(short center, short var);
