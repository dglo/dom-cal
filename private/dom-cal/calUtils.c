/*
 * calUtils.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
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
 * getFreqCalib
 *
 * Use calibration data to get sampling DAC setting, given the sampling 
 * frequency of an ATWD, in MHz
 *
 */
short getFreqCalib(int atwd, calib_data dom_calib, float sampling_freq) {
	short dac;
	
	/* Clock speed to sampling frequency */
	if (atwd == 0)
		dac = (-dom_calib.atwd0_freq_calib.c1 + 
			   sqrt((dom_calib.atwd0_freq_calib.c1 * dom_calib.atwd0_freq_calib.c1) - 
					(4.0 * dom_calib.atwd0_freq_calib.c2 * (dom_calib.atwd0_freq_calib.c0 - sampling_freq)))) / 
			  (2.0 * dom_calib.atwd0_freq_calib.c2);
	else
		dac = (-dom_calib.atwd1_freq_calib.c1 + 
			   sqrt((dom_calib.atwd1_freq_calib.c1 * dom_calib.atwd1_freq_calib.c1) - 
					(4.0 * dom_calib.atwd1_freq_calib.c2 * (dom_calib.atwd1_freq_calib.c0 - sampling_freq)))) / 
		(2.0 * dom_calib.atwd1_freq_calib.c2);
		
	return(dac);
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
 * pulserDAC2Q
 *
 * Converts the pulser amplitude DAC into a charge (in pC) at the FE.
 *
 */
float pulserDAC2Q(int pulser_dac) {
    return (pulser_dac * 0.0247);
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

/*
 * getDiscDAC
 *
 * Returns appropriate SPE discriminator DAC for given charge threshold
 *
 */
int getDiscDAC(float charge_threshold, calib_data dom_calib) {

    //q = m*DAC + b
    return (int)((charge_threshold - dom_calib.spe_disc_calib.y_intercept) /
                                              dom_calib.spe_disc_calib.slope);

}

/*---------------------------------------------------------------------------*/
/*
 * discV2DAC
 *
 * Inverse of discDAC2V -- convert a desired voltage into the 
 * required discriminator setting.  Clamps to a value between 0
 * and 1023.
 *
 */
int discV2DAC(float disc_v, int bias_dac) {
    int disc_dac = (int)(2.5 * (disc_v / (5.0 * 0.0000244) + 0.1 * bias_dac));
    disc_dac = disc_dac > 1023 ? 1023 : disc_dac;
    disc_dac = disc_dac < 0 ? 0 : disc_dac;
    return disc_dac;
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
 * getFEImpedance
 *
 * Returns FE impedance for given toroid type
 * New == 1 == 50 Ohms
 * Old == 0 == 43 Ohms
 *
 */
float getFEImpedance(short toroid_type) {
    return toroid_type ? 50.0 : 43.0;
}


/*---------------------------------------------------------------------------*/
/*
 * getFEImpedance
 *
 * Returns torroid type for a give impedance
 * New == 1 == 50 Ohms
 * Old == 0 == 43 Ohms
 *
 */
short getTorroidType(float fe_impedance) {
    return fe_impedance<50.0 ? 0 : 1;
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


/*---------------------------------------------------------------------------*/
/*
 * refineLinearFit
 *
 * Refine a linear fit by iteratively removing points with bad 
 * residuals.  Modifies x, y arrays, valid number of points, and linear
 * fit passed in.  
 *
 * minR2 is the minimum acceptable R^2 value (typically 0.99), minPts is
 * the number of points at which to stop if the R^2 target isn't reached.
 *
 * The vld array records which points were thrown out with a valid flag
 * for each of the original points.  If the calling function doesn't
 * need this, it can pass in NULL.
 *
 */
void refineLinearFit(float *x, float *y, int *vld_cnt, char *vld, 
                     linear_fit *fit, float minR2, int minPts) {

    int orig_cnt = *vld_cnt;

    /* Initialize valid array */
    int i, localVld;
    localVld = 0;
    if (vld == NULL) {
        vld = (char *)malloc(orig_cnt * sizeof(float));
        localVld = 1;
    }
    for (i = 0; i < orig_cnt; i++) 
        vld[i] = 1;

    /* X / Y arrays with bad points removed */
    float *x_refine = (float *)malloc(orig_cnt * sizeof(float));
    float *y_refine = (float *)malloc(orig_cnt * sizeof(float));
    if (!(x_refine) || !(y_refine)) return;

    /* Iterate until the R2 is high enough or we've run out of points */
    while ((fit->r_squared < minR2) && (*vld_cnt > minPts)) {
        
        /* Find worst point */
        int worst_idx = 0;
        float worst_residual = 0.0;
        float residual, y_calc;
        for (i = 0; i < orig_cnt; i++) {
            if (vld[i]) {
                y_calc = fit->slope * x[i] + fit->y_intercept;
                
                residual = fabs(y_calc - y[i]);
                
                if (residual > worst_residual) {
                    worst_residual = residual;
                    worst_idx = i;
                }                    
            }
        }
        
#ifdef DEBUG
        printf("R^2 value is too low (%g): discarding point %d\r\n",
               fit->r_squared, worst_idx);
#endif

        /* Discard that point */
        vld[worst_idx] = 0;
        int j = 0;
        for (i = 0; i < orig_cnt; i++) {
            if (vld[i]) {
                x_refine[j] = x[i];
                y_refine[j] = y[i];
                j++;
            }
        }

        (*vld_cnt)--;

        /* Try the fit again */
        linearFitFloat(x_refine, y_refine, *vld_cnt, fit);

    }

    free(x_refine);
    free(y_refine);
    if (localVld) 
        free(vld);
}

/*---------------------------------------------------------------------------*/
/*
 * sort -- do a standard heap sort on an array of floats
 */

void hsswap(float *data, int index1, int index2) {

  float d = data[index1];
  data[index1] = data[index2];
  data[index2] = d;

}

void hsrestore(float *data, int vertex, int limit) {

  int max_child;
  for (max_child = 2*vertex+1; max_child < limit; max_child = 2*vertex+1) {

    if (max_child + 1 < limit) {
      if (data[max_child+1] > data[max_child]) {
        max_child++;
      }
    }
    if (data[vertex] >= data[max_child]) return;
    hsswap(data, vertex, max_child);
    vertex = max_child;
  }
}

void sort(float *data, int cnt) {

  //Do std heap sort
  int cur_vertex;
  for (cur_vertex = cnt/2-1; cur_vertex >= 0; cur_vertex--) {
    hsrestore(data, cur_vertex, cnt);
  }
  for (cur_vertex = cnt-1; cur_vertex > 0; cur_vertex--) {
    hsswap(data, 0, cur_vertex);
    hsrestore(data, 0, cur_vertex);
  }
}

