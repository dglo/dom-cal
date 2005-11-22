/*
 * FADC cal header file 
 */
/* Which ATWD to use */
#ifdef DOMCAL_REV4
#define FADC_CAL_ATWD             1
#else
#define FADC_CAL_ATWD             0
#endif

/* Specific DAC values for this test */
#ifdef DOMCAL_REV5
#define FADC_CAL_PEDESTAL_DAC   2130
#define FADC_CAL_DISC_DAC        800
#define FADC_CAL_SAMPLING_DAC   2000 /* Higher than standard */
#else
#define FADC_CAL_PEDESTAL_DAC   1925
#define FADC_CAL_DISC_DAC        800
#define FADC_CAL_SAMPLING_DAC   2000
#endif

/* FADC ref DAC settings */
#define FADC_CAL_REF_MIN         725
#define FADC_CAL_REF_STEP         25
#define FADC_CAL_REF_CNT          12

/* Pulser amplitudes to use */
#define FADC_CAL_PULSER_AMP_MIN  600
#define FADC_CAL_PULSER_AMP_STEP 100
#define FADC_CAL_PULSER_AMP_CNT    4

/* Baseline setting (in FADC counts) to use */
#define FADC_CAL_BASELINE        100

/* ATWD bin to start looking for peak */
#define FADC_CAL_START_BIN        80

/* FADC bin to stop looking for peak */
#define FADC_CAL_STOP_SAMPLE      12

/* How many waveforms to measure */
#define FADC_CAL_TRIG_CNT        250

/* ATWD Integration window on either side of peak */
/* 16 bins *after* peak in time, 5 bins before peak */
/* related to sampling speed DAC */
#define FADC_CAL_ATWD_INT_WIN_MIN  16
#define FADC_CAL_ATWD_INT_WIN_MAX  5

/* FADC integration window -- until below this percent of peak */
#define FADC_CAL_INT_FRAC        0.1

/* Prototypes */
int fadc_cal(calib_data *dom_calib);
