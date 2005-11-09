/* 
 * domcal.h
 */

/* Print debugging information */
#define DEBUG 1

/* Version of calibration program -- Major version must
 * be incremented when changing structure of binary output
 */
#define MAJOR_VERSION 5
#define MINOR_VERSION 11

/* Default number of bytes in binary output */
#define DEFAULT_RECORD_LENGTH 9388

/* Default ATWD DAC settings */
#ifdef DOMCAL_REV5
#define ATWD_SAMPLING_SPEED_DAC  850
#define ATWD_RAMP_TOP_DAC       2300
#define ATWD_RAMP_BIAS_DAC       350
#define ATWD_ANALOG_REF_DAC     2250
#define ATWD_PEDESTAL_DAC       2130
#else
#define ATWD_SAMPLING_SPEED_DAC 850
#define ATWD_RAMP_TOP_DAC       2097
#define ATWD_RAMP_BIAS_DAC      3000
#define ATWD_ANALOG_REF_DAC     2048
#define ATWD_PEDESTAL_DAC       1925
#endif

/* Oscillator frequency into ATWD channel 3, mux input 0, in MHz */
#define DOM_CLOCK_FREQ          20.0

/* Wait time after setting a DAC */
#define DAC_SET_WAIT         1000000

/* Error codes */
#define FAILED_BINARY_CONVERSION -1;
#define FAILED_FLASH_WRITE -2;

/* Linear fit parameters */
typedef struct {
    float slope, y_intercept, r_squared;
} linear_fit;

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
    
    /* Date */
    short day, month, year;
    
    /* DOM ID */
    char dom_id[13];
    
    /* Calibration temperature, in K */
    float temp;

    /* DOM state before calibration */
    short dac_values[16];
    short adc_values[24];
   
    /* FADC calibration */
    short fadc_values[2];

    /* FE pulser calibration */
    linear_fit pulser_calib;

    /* ATWD gain calibration */
    linear_fit atwd0_gain_calib[3][128];
    linear_fit atwd1_gain_calib[3][128];
    
    /* ATWD baseline calibration (no HV) */
    float atwd0_baseline[3];
    float atwd1_baseline[3];

    /* FE amplifier calibration */
    value_error amplifier_calib[3];
    
    /* ATWD sampling speed calibration */
    linear_fit atwd0_freq_calib;
    linear_fit atwd1_freq_calib;

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

    /* Valid bit for HV baseline calibration */
    short hv_baselines_valid;

    /* Valid bit for PMT transit calibration */
    short transit_calib_valid;

    /* Histograms */
    hv_histogram* histogram_data;

} calib_data;
