/*
 * Header file for daq_baseline_cal
 */

/*  FPGA-related defines */
#define WHOLE_LBM_MASK         ((1<<24)-1)
#define FPGA_DOMAPP_LBM_BLOCKSIZE     2048 /* BYTES not words */

/* Number of triggers to record, and minimum number deemed clean */
#define DAQ_BASELINE_TRIG_CNT    300
#define DAQ_BASELINE_MIN_WF      100 

/* Maxiumum channel 1 variance (exclude pulses) */
#define DAQ_BASELINE_MAX_VAR  0.0005

/* Wait between forced triggers, in microsec */
#define DAQ_BASELINE_WAIT_US    1000

/* Flags to turn on/off LC and HV for the measurement */
#define DAQ_BASELINE_LC_ON         1
#define DAQ_BASELINE_HV_ON         1

#define DAQ_BASELINE_HV         1400

int daq_baseline_cal(calib_data *dom_calib);

