/* 
 * domcal.h
 */

/* Print debugging information */
#define DEBUG 1

/* Version of calibration program */
#define MAJOR_VERSION 0
#define MINOR_VERSION 2

/* Default ATWD DAC settings */
#define ATWD_SAMPLING_SPEED_DAC 850
#define ATWD_RAMP_TOP_DAC       2097
#define ATWD_RAMP_BIAS_DAC      3000
#define ATWD_ANALOG_REF_DAC     2048
#define ATWD_PEDESTAL_DAC       1925

/* Linear fit parameters */
typedef struct {
    float slope, y_intercept, r_squared;
} linear_fit;

/* Store value and error together */
typedef struct {
    float value, error;
} value_error;

/* Calibration data structure */
typedef struct {
    
    /* Date */
    int day, month, year;
    
    /* DOM ID */
    char dom_id[12];
    
    /* Calibration temperature */
    short temp;

    /* DOM state before calibration */
    short dac_values[16];
    short adc_values[24];
    short fadc_values[4];

    /* FE pulser calibration */
    linear_fit pulser_calib;

    /* ATWD gain calibration */
    linear_fit *atwd0_gain_calib[4][128];
    linear_fit *atwd1_gain_calib[4][128];
    
    /* FE amplifier calibration */
    value_error *amplifier_calib[3];
    
    /* ATWD sampling speed calibration */
    linear_fit atwd0_freq_calib;
    linear_fit atwd1_freq_calib;

} calib_data;
