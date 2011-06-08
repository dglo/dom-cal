/*
 * pulser_cal header file
 */

/* Number of SPE threshold settings to fit */
#define NUMBER_OF_SPE_SETTINGS  11

/* Pedestal value setting for pulser calibration */
#ifdef DOMCAL_REV5
#define PEDESTAL_VALUE          2130
#else
#define PEDESTAL_VALUE          1925
#endif

/* Error code indicating correct amplitude could not be localized */
#define AMPLITUDE_NOT_LOCALIZED 2

/* Prototypes */
int pulser_cal(calib_data *dom_calib);
