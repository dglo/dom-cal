/*
 * Header file delta_t_cal
 */

/* Which ATWD to use */
#ifdef DOMCAL_REV4
#define DELTA_T_ATWD                     1
#else
#define DELTA_T_ATWD                     0
#endif

/* Specific DAC values for this test */
#ifdef DOMCAL_REV5
#define DELTA_T_PEDESTAL_DAC          2130
#define DELTA_T_DISC_DAC               800
#define DELTA_T_SAMPLING_DAC          2000 /* Higher than standard */
#define DELTA_T_FADC_REF               800
#else
#define DELTA_T_PEDESTAL_DAC          1925
#define DELTA_T_DISC_DAC               800
#define DELTA_T_SAMPLING_DAC          2000
#define DELTA_T_FADC_REF               800
#endif

/*  FPGA-related defines */
#define WHOLE_LBM_MASK         ((1<<24)-1)
#define FPGA_DOMAPP_LBM_BLOCKSIZE     2048 /* BYTES not words */

/* Number of triggers to record, and minimum number deemed clean */
#define DELTA_T_TRIG_CNT               300
#define DELTA_T_MIN_WF                 100 

/* Wait between forced triggers, in microsec */
#define DELTA_T_WAIT_US               1000

/* Pulser rate, in Hz */
#define DELTA_T_PULSER_RATE_HZ        1200

/* Pulser amplitudes to use */
#define DELTA_T_PULSER_AMP_MIN         600
#define DELTA_T_PULSER_AMP_STEP        100
#define DELTA_T_PULSER_AMP_CNT           4

/* ATWD bin to start looking for peak */
#define DELTA_T_START_BIN               80

/* FADC bin to stop looking for peak */
#define DELTA_T_STOP_SAMPLE             12

int delta_t_cal(calib_data *dom_calib);

