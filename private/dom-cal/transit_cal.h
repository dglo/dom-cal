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

/* Which ATWD to use */
#define TRANSIT_CAL_ATWD              0

/* Which channel to use */
#define TRANSIT_CAL_CH                1

/* Flasher brightness */
/* 600 is about the maximim reliable value */
/* (minimum brightness) */
#define TRANSIT_CAL_LED_AMPLITUDE   600

/* How many waveforms to measure */
#define TRANSIT_CAL_TRIG_CNT        250

/* Trigger delay for ATWDs */
#define TRANSIT_CAL_LAUNCH_DELAY      2

/* Prototypes */
int transit_cal(calib_data *dom_calib);
