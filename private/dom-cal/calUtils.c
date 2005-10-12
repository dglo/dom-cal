/*
 * calUtils.c
 *
 */

#include <math.h>
#include <string.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "calUtils.h"

/*---------------------------------------------------------------------------*/
/*
 * getCalibV
 *
 * Use calibration data to convert ATWD value to input voltage, before channel
 * amplification.
 *
 */

float getCalibV(short val, calib_data dom_calib, int atwd, int ch, int bin, float bias_v) {

    float v;

    /* Using ATWD calibration data, convert to V */
    if (atwd == 0) {
        v = (float)val * dom_calib.atwd0_gain_calib[ch][bin].slope
            + dom_calib.atwd0_gain_calib[ch][bin].y_intercept;
    }
    else {
        v = (float)val * dom_calib.atwd1_gain_calib[ch][bin].slope
            + dom_calib.atwd1_gain_calib[ch][bin].y_intercept;        
    }
        
    /* Also subtract out bias voltage */
    v -= bias_v;
    
    /* Correct for channel amplification with amplifier calibration data */
    v /= dom_calib.amplifier_calib[ch].value;
       
    return v;
        
}

/*---------------------------------------------------------------------------*/
/*
 * getCalibFreq
 *
 * Use calibration data to get sampling frequency of an ATWD, in MHz
 *
 */
float getCalibFreq(int atwd, calib_data dom_calib, short sampling_dac) {

    float freq;

    /* Sampling frequency to clock speed */
    if (atwd == 0)
        freq = dom_calib.atwd0_freq_calib.c0 +
            (dom_calib.atwd0_freq_calib.c1 * sampling_dac) + 
            (dom_calib.atwd0_freq_calib.c2 * sampling_dac * sampling_dac);
    else
        freq = dom_calib.atwd1_freq_calib.c0 +
            (dom_calib.atwd1_freq_calib.c1 * sampling_dac) + 
            (dom_calib.atwd1_freq_calib.c2 * sampling_dac * sampling_dac);
    
    return freq;
}

/*---------------------------------------------------------------------------*/
/*
 * temp2K
 *
 * Converts a raw DOM temperature reading to Kelvin.
 *
 */
float temp2K(short temp) {
    return (temp/256.0 + 273.16);
}

/*---------------------------------------------------------------------------*/
/*
 * discDAC2V
 *
 * Converts the discriminator DAC setting to volts (using bias DAC too).
 *
 */
float discDAC2V(int disc_dac, int bias_dac) {
    return (0.0000244 * (0.4 * disc_dac - 0.1 * bias_dac) * 5.0);
}

/*---------------------------------------------------------------------------*/
/*
 * biasDAC2V
 *
 * Converts the FE pedestal level (bias) from DAC value to volts
 *
 */
float biasDAC2V(int val) {
    return (val * 5.0 / 4096.0);
}

/*---------------------------------------------------------------------------*/
/* 
 * prescanATWD
 *
 * Call before any data acquisition to "prime" the ATWDs.
 */
void prescanATWD(unsigned int trigger_mask) {
   int i;

   for (i=0; i<8; i++) {
      hal_FPGA_TEST_trigger_forced(trigger_mask);
      while (!hal_FPGA_TEST_readout_done(trigger_mask)) ;
      halUSleep(1000);
   }
}


/*---------------------------------------------------------------------------*/
/*
 * checkHVBase
 *
 * Check to make sure there *is* a HV base by looking at ID
 * Avoids running on, say, the sync board
 *
 * Assume base has already been enabled 
 */
int checkHVBase() {
    const char *hv_id = halHVSerial();
    return strcmp(hv_id, "000000000000") == 0 ? 0 : 1;
}

/*---------------------------------------------------------------------------*/
/*
 * meanVarFloat
 *
 * Finds the mean and variance of an array of floats.
 *
 */
void meanVarFloat(float *x, int pts, float *mean, float *var) {

    int i;
    /* Use double internally */
    double sum = 0.0;
    
    for (i = 0; i < pts; i++)
        sum += x[i];

    *mean = sum / pts;

    sum = 0.0;
    for (i = 0; i < pts; i++)
        sum += (x[i] - *mean)*(x[i] - *mean);

    *var = (float)(sum / pts);

}

/*---------------------------------------------------------------------------*/
/*
 * linearFitFloat
 *
 * Linear regression of arrays of floats.
 *
 * Takes an array of (x,y) points and returns the 
 * slope, intercept, and R-squared value of the best-fit
 * line, using the linear_fit struct in domcal.h.
 *
 */
void linearFitFloat(float *x, float *y, int pts, linear_fit *fit) {

    int i;
    /* Use doubles internally */
    double sum_x, sum_y;
    double sum_xy, sum_xx, sum_yy;
    double denom;

    sum_x = sum_y = 0.0;
    sum_xy = sum_xx = sum_yy = 0.0;

    for (i=0; i<pts; i++) {        
        sum_x  += x[i];
        sum_xx += x[i]*x[i];
        sum_y  += y[i];        
        sum_yy += y[i]*y[i];
        sum_xy += x[i]*y[i];     
    }

    denom = pts*sum_xx - sum_x*sum_x;

    fit->slope = (float)(pts*sum_xy - sum_x*sum_y) / denom;
    fit->y_intercept = (float)(sum_xx*sum_y - sum_x*sum_xy) / denom;
    fit->r_squared = (float)(pts*sum_xy - sum_x*sum_y) * (fit->slope) / 
        (pts*sum_yy - sum_y*sum_y);

}

/*---------------------------------------------------------------------------*/
/*
 * quadraticFitFloat
 *
 * Quadratic least-squares regression of arrays of floats.
 *
 * Takes an array of (x,y) points and returns the 
 * constant, linear, and quadratic coefficients for the least-squares
 * fit.
 *
 */
void quadraticFitFloat(float *x, float *y, int pts, quadratic_fit *fit) {

    int i;

    double x1, x2, x3, x4;
    double y1, y2, y3;

    x1 = x2 = x3 = x4 = 0.0;
    y1 = y2 = y3 = 0.0;

    for (i = 0; i < pts; i++) {
        x1 += x[i];
        x2 += x[i]*x[i];
        x3 += x[i]*x[i]*x[i];
        x4 += x[i]*x[i]*x[i]*x[i];

        y1 += y[i];
        y2 += y[i]*x[i];
        y3 += y[i]*x[i]*x[i];
    }

    double denom = x2*x2*x2 + pts*x3*x3 + x1*x1*x4 - x2*(2*x1*x3 + pts*x4);

    /* Here are the fit coefficients */
    fit->c0 = (x3*x3*y1 - x2*x4*y1 + x1*x4*y2 + x2*x2*y3 - x3*(x2*y2 + x1*y3)) / denom;
    fit->c1 = (x1*x4*y1 + x2*x2*y2 - pts*x4*y2 + pts*x3*y3 - x2*(x3*y1 + x1*y3)) / denom;
    fit->c2 = (x2*x2*y1 - x1*x3*y1 + pts*x3*y2 + x1*x1*y3 - x2*(x1*y2 + pts*y3)) / denom;

    /* Calculate the correlation coefficient */
    float res, sq_res;
    float y_sq_res = 0.0;
    sq_res = 0.0;

    float y_mean = y1 / pts;    
    for (i = 0; i < pts; i++) {
        res = y_mean - (fit->c0 + fit->c1*x[i] + fit->c2*x[i]*x[i]);
        sq_res += res*res;
        y_sq_res += (y[i] - y_mean)*(y[i] - y_mean);
    }

    fit->r_squared = sq_res / y_sq_res;
}
