/* 
 * domcal.h
 */

/* Print debugging information */
#define DEBUG 1

/* Version of calibration program -- Major version must
 * be incremented when changing structure of binary output
 */
#define MAJOR_VERSION 2
#define MINOR_VERSION 0

/* Default number of bytes in binary output */
#define DEFAULT_RECORD_LENGTH 9386

/* Default ATWD DAC settings */
#define ATWD_SAMPLING_SPEED_DAC 850
#define ATWD_RAMP_TOP_DAC       2097
#define ATWD_RAMP_BIAS_DAC      3000
#define ATWD_ANALOG_REF_DAC     2048
#define ATWD_PEDESTAL_DAC       1925

/* Mainboard oscillator frequency into ATWD channel 3, in MHz */
#ifdef REV3HAL
#define DOM_CLOCK_FREQ          40.0
#else
#define DOM_CLOCK_FREQ          20.0
#endif

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

/* Store P/V and voltage together */
typedef struct {
    float pv, voltage;
} pv_dat;

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
    
    /* FE amplifier calibration */
    value_error amplifier_calib[3];
    
    /* ATWD sampling speed calibration */
    linear_fit atwd0_freq_calib;
    linear_fit atwd1_freq_calib;

    /* Valid bit for HV calibration */
    short hv_gain_valid;

    /* Log(HV) vs. Log(gain) HV calibration fit */
    linear_fit hv_gain_calib;

    /* Number of P/V results returned */
    short num_pv;

    /* P/V results */
    pv_dat* pv_data;

} calib_data;
