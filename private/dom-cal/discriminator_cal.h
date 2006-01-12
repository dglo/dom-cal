/*
 * discriminator_cal header file
 */

/* Disc settings to calibrate */
#define DISC_CAL_CNT  10
#define DISC_CAL_MIN  550
#define SPE_DISC_CAL_INC  50
#define MPE_DISC_CAL_INC  10 //smaller due to limited charge range of pulser

/* Pedestal value setting for pulser calibration */
#ifdef DOMCAL_REV5
#define PEDESTAL_VALUE          2130
#else
#define PEDESTAL_VALUE          1925
#endif

/* Error code indicating correct amplitude could not be localized */
#define AMPLITUDE_NOT_LOCALIZED 2

/* Prototypes */
int disc_cal(calib_data *dom_calib);
