/*
 * calUtils.c
 *
 */

#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "calUtils.h"

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
 * meanVarFloat
 *
 * Finds the mean and variance of an array of floats.
 *
 */
void meanVarFloat(float *x, int pts, float *mean, float *var) {

    int i;
    float sum = 0.0;
    
    for (i = 0; i < pts; i++)
        sum += x[i];

    *mean = sum / (float)pts;

    sum = 0.0;
    for (i = 0; i < pts; i++)
        sum += (x[i] - *mean)*(x[i] - *mean);

    *var = sum / (float)pts;

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
    float sum_x, sum_y;
    float sum_xy, sum_xx, sum_yy;
    float denom;

    sum_x = sum_y = 0.0;
    sum_xy = sum_xx = sum_yy = 0.0;

    for (i=0; i<pts; i++) {        
        sum_x  += x[i];
        sum_xx += x[i]*x[i];
        sum_y  += y[i];        
        sum_yy += y[i]*y[i];
        sum_xy += x[i]*y[i];     
    }

    denom = (float)(pts*sum_xx - sum_x*sum_x);

    fit->slope = (float)(pts*sum_xy - sum_x*sum_y) / denom;
    fit->y_intercept = (float)(sum_xx*sum_y - sum_x*sum_xy) / denom;
    fit->r_squared = (float)(pts*sum_xy - sum_x*sum_y) * (fit->slope) / 
        (pts*sum_yy - sum_y*sum_y);

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
