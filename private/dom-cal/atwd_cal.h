/*
 * atwd_cal header file
 */

/* FE bias level values for calibration */
#define BIAS_LOW  1000
#define BIAS_HIGH 2000
#define BIAS_STEP  100
/* Redundant, but useful -- number of data points, including BIAS_HIGH */
#define BIAS_CNT    11

/* How many CPU triggers to average for each pedestal */
#define ATWD_CAL_TRIG_CNT 100

/* Prototypes */
int atwd_cal(void);
