/*
 * atwd_cal header file
 */

/* FE bias level values for calibration */
#ifdef DOMCAL_REV5
#define BIAS_LOW  1200
#define BIAS_HIGH 2200
#define BIAS_STEP  100
#else
#define BIAS_LOW  1000
#define BIAS_HIGH 2000
#define BIAS_STEP  100
#endif
/* Redundant, but useful -- number of data points, including BIAS_HIGH */
#define BIAS_CNT    11

/* How many CPU triggers to average for each pedestal */
#define ATWD_CAL_TRIG_CNT 100

/* Prototypes */
int atwd_cal(calib_data *dom_calib);
