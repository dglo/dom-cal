/* 
 * domcal.h
 */

#ifndef _DOMCAL_H_
#define _DOMCAL_H_
/* Print debugging information */
#define DEBUG 1

/* Version of calibration program */
#define MAJOR_VERSION 7
#define MINOR_VERSION 0
#define PATCH_VERSION 7

/* Default ATWD DAC settings */
#ifdef DOMCAL_REV5
#define ATWD_SAMPLING_SPEED_DAC  850
#define ATWD_RAMP_TOP_DAC       2300
#define ATWD_RAMP_BIAS_DAC       350
#define ATWD_ANALOG_REF_DAC     2250
#define ATWD_PEDESTAL_DAC       2130
#define FAST_ADC_REF             800
#else
#define ATWD_SAMPLING_SPEED_DAC 850
#define ATWD_RAMP_TOP_DAC       2097
#define ATWD_RAMP_BIAS_DAC      3000
#define ATWD_ANALOG_REF_DAC     2048
#define ATWD_PEDESTAL_DAC       1925
#define FAST_ADC_REF            800
#endif

/* Oscillator frequency into ATWD channel 3, mux input 0, in MHz */
#define DOM_CLOCK_FREQ          20.0

/* FADC sampling frequency, in MHz */
#define FADC_CLOCK_FREQ         40.0

/* Wait time after setting a DAC */
#define DAC_SET_WAIT         1000000

/* Initial discriminator DAC setting */
#ifdef DOMCAL_REV5
#define DISC_DAC_INIT   556
#else
#define DISC_DAC_INIT   505
#endif


/* Linear fit parameters */
typedef struct {
    float slope, y_intercept, r_squared;
} linear_fit;

/* Quadratic fit parameters */
typedef struct {
    float c0, c1, c2, r_squared;
} quadratic_fit;

/* Store value and error together */
typedef struct {
    float value, error;
} value_error;

/* HV histogram */
typedef struct {
    float* x_data;
    float* y_data;
    short voltage;
    short bin_count;
    short convergent;
    float* fit;
    float pv;
    float noise_rate;
    short is_filled;
} hv_histogram;

/* HV baselines */
typedef struct {
    short voltage;
    float atwd0_hv_baseline[3];
    float atwd1_hv_baseline[3];
} hv_baselines;

/* Calibration data structure */
typedef struct {

    /* Front end impedance */
    float fe_impedance;
    
    /* Date */
    short day, month, year;

    /* Time */
    short hour, minute, second;

    /* DOM ID */
    char dom_id[13];
    
    /* Calibration temperature, in K */
    float temp;

    /* DOM state before calibration */
    short dac_values[16];
    short adc_values[24];

    /* Maximum allowed HV */
    short max_hv;
   
    /* FADC calibration */
    linear_fit  fadc_baseline;
    value_error fadc_gain;
    value_error fadc_delta_t;

    /* FE pulser calibration */
    /* OBSOLETE as of v6.0 */
    linear_fit pulser_calib;

    /* discriminator calibration */
    linear_fit spe_disc_calib;
    linear_fit mpe_disc_calib;

    /* ATWD gain calibration */
    linear_fit atwd0_gain_calib[3][128];
    linear_fit atwd1_gain_calib[3][128];
    
    /* ATWD baseline calibration (no HV) */
    float atwd0_baseline[3];
    float atwd1_baseline[3];

    /* FE amplifier calibration */
    value_error amplifier_calib[3];
    
    /* ATWD sampling speed calibration */
    quadratic_fit atwd0_freq_calib;
    quadratic_fit atwd1_freq_calib;

    /* DAQ baseline waveforms (domapp FPGA) */
    short daq_baselines_valid;
    float atwd0_daq_baseline_wf[3][128];
    float atwd1_daq_baseline_wf[3][128];

    /* Valid bit for HV calibration */
    short hv_gain_valid;

    /* HV baselines */
    hv_baselines* baseline_data;

    /* Log(HV) vs. Log(gain) HV calibration fit */
    linear_fit hv_gain_calib;

    /* transit time vs hv array */
    linear_fit transit_calib;

    /* Number of histograms returned */
    short num_histos;

    /* Number of HV baselines recorded (number of HV points) */
    short num_baselines;

    /* Valid bit for HV baseline calibration */
    short hv_baselines_valid;

    /* Valid bit for PMT transit calibration */
    short transit_calib_valid;

    /* Number of valid transit time points */
    short transit_calib_points;

    /* Histograms */
    hv_histogram* histogram_data;

} calib_data;
#endif
