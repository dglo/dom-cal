/*
 * transit_cal header file
 */

/* HV settings for gain calibration (in V) */
/* Starting value, amount to increment, and number of settings */
#define TRANSIT_CAL_HV_LOW        1100
#define TRANSIT_CAL_HV_INC         100
#define TRANSIT_CAL_HV_CNT           9

/* Specific DAC values for this test */
/* Note high sampling speed! */
#ifdef DOMCAL_REV5
#define TRANSIT_CAL_PEDESTAL_DAC   2130
#define TRANSIT_CAL_SAMPLING_DAC   2100
#else
#define TRANSIT_CAL_PEDESTAL_DAC   1925
#define TRANSIT_CAL_SAMPLING_DAC   2100
#endif

/* Which ATWD to use */
#define TRANSIT_CAL_ATWD              0

/* LED brightness starting/stopping values */
#define TRANSIT_CAL_LED_AMP_START   950
#define TRANSIT_CAL_LED_AMP_STOP    450
#define TRANSIT_CAL_LED_AMP_STEP      3

/* Which channel to use */
#define TRANSIT_CAL_CH                1

/* Number of triggers to average for amplitude check */
#define TRANSIT_CAL_AMP_TRIG         20

/* Amplitude to look for in channel TRANSIT_CAL_CH */
#define TRANSIT_CAL_ATWD_AMP_LOW    400   
#define TRANSIT_CAL_ATWD_AMP_HIGH   800 

/* How many waveforms to measure */
#define TRANSIT_CAL_TRIG_CNT        250

/* Trigger delay for ATWDs */
#define TRANSIT_CAL_LAUNCH_DELAY      4

/* Crossing point to define transit time */
/* As fraction of peak value */
#define TRANSIT_CAL_EDGE_FRACT      0.5

/* Maxiumum number of waveforms with no light */
#define TRANSIT_CAL_MAX_NO_PEAKS    750

/* Maxiumum number of waveforms with bad leading edge */
#define TRANSIT_CAL_MAX_BAD_LE      750

/* Minimum number of points to attempt a fit */
#define TRANSIT_CAL_MIN_VLD_PTS       2

/* Maximum sigma, in ns, of transit time at a given HV */
/* Otherwise don't accept the point for fit */
#define TRANSIT_CAL_MAX_SIGMA       5.0

/* Lowest acceptable R^2 */
#define TRANSIT_CAL_MIN_R2          0.99

/* Minimum points to leave when improving R^2 */
#define TRANSIT_CAL_MIN_R2_PTS        4

/* Error code for no HV base */
#define TRANSIT_CAL_NO_HV_BASE        2

/* Error code for variance failure */
#define TRANSIT_CAL_NO_LIGHT_ERR      3

/* Error code for brightness search failure */
#define TRANSIT_CAL_AMP_ERR           4

/* Error code for too few valid points */
#define TRANSIT_CAL_PTS_ERR           5

/* Prototypes */
int transit_cal(calib_data *dom_calib);
