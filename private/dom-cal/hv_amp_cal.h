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

/* Prototypes */
int hv_amp_cal(calib_data *dom_calib);
