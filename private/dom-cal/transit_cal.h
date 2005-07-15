/*
 * transit_cal header file
 */

/* HV settings for gain calibration (in V) */
/* Starting value, amount to increment, and number of settings */
#define TRANSIT_CAL_HV_LOW        1200
#define TRANSIT_CAL_HV_INC         100
#define TRANSIT_CAL_HV_CNT           8

/* Specific DAC values for this test */
#ifdef DOMCAL_REV5
#define TRANSIT_CAL_PEDESTAL_DAC   2130
#define TRANSIT_CAL_SAMPLING_DAC    850
#else
#define TRANSIT_CAL_PEDESTAL_DAC   1925
#define TRANSIT_CAL_SAMPLING_DAC    850
#endif

/* UNCOMMENT TO USE FLASHERBOARD */
/* Otherwise, we use the mainboard LED */
/* #define TRANSIT_CAL_USE_FB */

/* Which ATWD to use */
#define TRANSIT_CAL_ATWD              0

#ifdef TRANSIT_CAL_USE_FB

/* Flasher board current reference DAC */
#define TRANSIT_CAL_FLASHER_REF     450

/* Which LED to use */
#define TRANSIT_CAL_FB_LED            2

/* Width of FB pulse */
#define TRANSIT_CAL_FB_WIDTH         40

/* Rate to flash, in Hz */
#define TRANSIT_CAL_FB_RATE          50

/* Flasher brightness starting value */
#define TRANSIT_CAL_FB_AMP_START      0
#define TRANSIT_CAL_FB_AMP_STOP     127
#define TRANSIT_CAL_FB_AMP_STEP       2

/* Which channel to use */
#define TRANSIT_CAL_CH                2

#else
/* MAINBOARD LED DEFINES */

/* LED brightness starting/stopping values */
#define TRANSIT_CAL_LED_AMP_START   700
#define TRANSIT_CAL_LED_AMP_STOP    550
#define TRANSIT_CAL_LED_AMP_STEP      3

/* Which channel to use */
#define TRANSIT_CAL_CH                1

#endif

/* Number of triggers to average for amplitude check */
#define TRANSIT_CAL_AMP_TRIG         20

/* Amplitude to look for in channel TRANSIT_CAL_CH */
#define TRANSIT_CAL_ATWD_AMP_LOW    400   
#define TRANSIT_CAL_ATWD_AMP_HIGH   800 

/* How many waveforms to measure */
#define TRANSIT_CAL_TRIG_CNT        250

/* Trigger delay for ATWDs */
#define TRANSIT_CAL_LAUNCH_DELAY      2

/* Crossing point to define transit time */
/* As fraction of peak value */
#define TRANSIT_CAL_EDGE_FRACT      0.5

/* Maxiumum number of waveforms with no light */
#define TRANSIT_CAL_MAX_NO_PEAKS    750

/* How many times we will crank up the brightness */
#define TRANSIT_CAL_MAX_NO_LIGHT_CNT   5

/* Error code for no HV base */
#define TRANSIT_CAL_NO_HV_BASE        2

/* Error code for variance failure */
#define TRANSIT_CAL_NO_LIGHT_ERR      3

/* Error code for brightness search failure */
#define TRANSIT_CAL_AMP_ERR           4

/* Prototypes */
int transit_cal(calib_data *dom_calib);
