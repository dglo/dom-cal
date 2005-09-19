#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "domcal.h"
#include "spefit.h"
#include "lmfit.h"
#include "calUtils.h"

/*---------------------------------------------------------------------------*/
/*
 * f_spe
 *
 * Function passed to nonlinear fit routine -- returns value and Jacobian
 * entries for SPE fit function.
 *
 */

void f_spe(float x, float *a, float *y, float *dyda, int nparam) {
                                                                                
    float e1, e2, xoff;
                                                                                
    /* Model Yi = A * exp(-B * x) + C * exp( -(x - D)^2 * E ) */
                                                                                
    xoff = x - a[3];
    e1 = exp(-a[1] * x);
    e2 = exp(-xoff * xoff * a[4]);
                                                                                
    /* Value of function */
    *y = (a[0] * e1)+ (a[2] * e2);
                                                                                
    /* Jacobian wrt parameters */
    dyda[0] = e1;
    dyda[1] = -a[0] * x * e1;
    dyda[2] = e2;
    dyda[3] = 2 * a[2] * xoff * e2 * a[4];
    dyda[4] = -a[2] * xoff * xoff * e2;

}

/*---------------------------------------------------------------------------*/
/*
 * get_fit_initialization
 *
 * Best guess to charge histogram fit parameter initialization.
 *
 */

void get_fit_initialization( float *x, float *y, int num, float *params ) {
	    
    /* Find mean and variance */
    float sum = 0;
    float histmax = y[0];
    int i;
    for ( i = 0; i < num; i++ ) {
        /* we know y values are actually integers */
        sum += y[i];
	if (y[i] > histmax) histmax = y[i];
    }

    /* Gaussian amplitude */
    params[2] = histmax;

    float xvals[(int)sum];

    int j;
    int indx = 0;
    for ( i = 0; i < num; i++ ) {
        for ( j = 0; j < y[i]; j++ ) {
            xvals[indx] = x[i];
            indx++;
        }
    }

    float mean, variance;
    meanVarFloat( xvals, ( int )sum , &mean, &variance );

    /* Exponential decay rate */
    params[1] = mean / 4.0;

    /* Exponential amplitude */
    params[0] = y[0];
    /* Zero amplitude will crash fit! */
    if (params[0] == 0.0) params[0] = 0.01;

    /* Gaussian center */
    params[3] = mean;

    /* Gaussian width */
    params[4] = 1.0 / ( 2 * variance );

}

/*--------------------------------------------------------------------------*/
/*
 * spe_find_valley
 *
 * Find the valley (first minimum) of an SPE fit function 
 * using a Newton-Raphson search on the first derivative.
 *
 */
int spe_find_valley(float *a, float *valley_x, float *valley_y) {

    float e1, e2, xoff;
    float x, d1f, d2f;

    /* Model Yi = A * exp(-B * x) + C * exp( -(x - D)^2 * E ) */
     
    /* Find valley (first minimum) with Newton-Raphson search for zero
       of first derivative */

    /* First guess -- set exponential and gaussian equal */
    float root = sqrt(a[1]*a[1] + 4*a[3]*a[4]*a[1] - 4*a[4]*log(a[0]/a[2]));
    x = (a[1] + 2*a[3]*a[4] - root) / (2 * a[4]);


    int iter = 0;
    int done = 0;
    int converged = 0;
    int err = 0;

    while (!done) {

        xoff = x - a[3];
        e1 = exp(-a[1] * x);
        e2 = exp(-xoff * xoff * a[4]);

        /* First derivative of SPE function: */
        /* -A B exp(-B x) - 2 C E (x-d) exp (-E (x-d)^2) */
        d1f = (-a[0] * a[1] * e1) - (2 * a[2] * a[4] * xoff* e2);

        converged = (fabs(d1f) < SPE_FIT_NR_MAX_ERR);

        if ((!converged) && (iter < SPE_FIT_NR_MAX_ITER)) {

            /* Second derivative of SPE function: */
            /* 
             * A B^2 exp(-B x) - 2 C exp(-E (x-D)^2) E + 
             *    4 C E^2 (x-D)^2 exp(-E (x-D)^2)
             */
            d2f = (a[0] * a[1] * a[1] * e1) - (2 * a[2] * a[4] * e2)
                + (4 * a[2] * a[4] * a[4] * xoff * xoff * e2);            

            x -= d1f / d2f;
            iter++;
        }
        else done = 1;

    }

    *valley_x = x;
    *valley_y = (a[0] * e1) + (a[2] * e2);

#ifdef DEBUG
    printf("Possible valley at %f\r\n", *valley_x);
#endif

    if(!converged)
        err = SPE_FIT_ERR_NR_NO_CONVERGE;

    /* Check sanity of valley */
    /* Should be positive and to the left of the gaussian peak */
    if ((*valley_x < 0) || (*valley_x > a[3]))
        err = SPE_FIT_ERR_NR_BAD_X;

    return err;
}

/*--------------------------------------------------------------------------*/
int spe_fit(float *xdata, float *ydata, int pts,
            float *fit_params, int num_samples ) {

    int i, ndata;
    int iter = 0;
    int err = 0;

    /* Working matrices */
    float **alpha = matrix(SPE_FIT_PARAMS, SPE_FIT_PARAMS);
    float **covar = matrix(SPE_FIT_PARAMS, SPE_FIT_PARAMS);
    
    /* Chi squared */
    float chisq = 0.0;

    /* Must be negative for first iteration */
    float lambda = -1.0;
    
    /* Find a maximal bin with > 1.5% of total hits */
    int start_bin = 0;    
    int nonzero_bin = 0;
    int nonzero_found = 0;

    while ((ydata[start_bin] < (SPE_FIT_NOISE_FRACT * num_samples) || 
           (ydata[start_bin] < ydata[start_bin + 1])) && 
           (start_bin < pts - 1)) {

        /* Also record first non-zero bin as a fallback */
        if ((!nonzero_found) && (ydata[start_bin] > 0)) {
            nonzero_found = 1;
            nonzero_bin = start_bin;
        }

        start_bin++;
    }

    /* if start_bin > pts / 20, we're better off starting at first nonzero bin + 2 -- +2 to get rid of disc effects*/
    if ( start_bin > pts / 20 )
        start_bin = ydata[nonzero_bin + 2] > ydata[nonzero_bin] ? nonzero_bin + 2 : nonzero_bin;

    /*  OK -- let's chop off the last few % -- these are probably non-gaussian */
    ndata = pts;
    int tot = 0;
    for (; ndata > start_bin; ndata--) {
        tot += ydata[ndata-1];
        if (tot > SPE_BAD_TAIL_FRACTION * num_samples) break;
    }

    /* Determine the number of data points */
    ndata -= start_bin;

    /* Check to make sure histogram isn't empty */
    if (ndata == 0) {
#ifdef DEBUG
        printf("SPE fit error: empty histogram!\r\n");        
#endif
        return SPE_FIT_ERR_EMPTY_HIST;
    }

#ifdef DEBUG
    printf("Starting fit at bin %d\r\n", start_bin);
#endif
    
    /* Determine the sigma at each point */  
    float *sigma = vector(ndata);
    for (i = 0; i < ndata; i++) {
            /* Std. dev. estimate for each point */
            /* sigma[i] = sqrt(ydata[i+startbin]); */
            /* Shouldn't be < 1.0 */
            /* sigma[i] = (sigma[i] < 1.0) ? 1.0 : sigma[i]; */

            /* TEMP */
            sigma[i] = 0.1;
    }
  
    /* Get starting fit parameters */
    get_fit_initialization( &xdata[start_bin], &ydata[start_bin],
                            ndata, fit_params);

#ifdef DEBUG
    printf("Fitting SPE histogram...\r\n");
#endif

    /* Fit loop */
    int done = 0;
    int converged = 0;
    float old_chisq = chisq;
    float del_chisq;

    while (!done) {

        /* Loop is done when the max number of iterations is reached */
        /* or when chi-squared has decreased, but only by a small amount */
        /* Decrease can be small absolutely or as a percentage */

        /* FIX ME DEBUG */
        /* print current parameters */
        printf("Parameter values, iteration %d:\r\n", iter);
        printf("Chisq: %.6g\r\n", chisq);
        for (i = 0; i < SPE_FIT_PARAMS; i++)
            printf(" a[%d] = %g\n", i, fit_params[i]);

        iter++;
        del_chisq = old_chisq - chisq;
        converged = (del_chisq > 0) && ((del_chisq < SPE_FIT_CHISQ_ABS_DONE) ||
                                        (del_chisq / chisq < SPE_FIT_CHISQ_PCT_DONE));

        done = (iter > SPE_FIT_MAX_ITER) || converged;
        
        /* Setting to zero on final iteration will read out covariance and curvature */
        if (done)
            lambda = 0.0;

        /* Save chi-squared */
        old_chisq = chisq;

        /* Do an iteration and watch for errors */
        if (lmfit(&xdata[start_bin], &ydata[start_bin], sigma, ndata,
                  fit_params, SPE_FIT_PARAMS, covar,
                  alpha, &chisq, f_spe, &lambda)) {

            done = 1;
            err = SPE_FIT_ERR_SINGULAR;
#ifdef DEBUG
            printf("SPE fit error: singular matrix\r\n");
#endif
        };
    }

    /* Print values */
#ifdef DEBUG
    printf("Final parameter values, iteration %d:\r\n", iter);
    printf("Chisq: %.6g\r\n", chisq);
    for (i = 0; i < SPE_FIT_PARAMS; i++)
        printf(" a[%d] = %g\n", i, fit_params[i]);
#endif

    /* Read out covariance matrix */
    free_vector(sigma);
    free_matrix(alpha, SPE_FIT_PARAMS);
    free_matrix(covar, SPE_FIT_PARAMS);

    /* Check fit for convergence and sanity */
    if (converged) {
#ifdef DEBUG
        printf("SPE fit converged successfully\r\n");
#endif
        /* Check that all fit parameters are positive and sane */
        for (i = 0; i < SPE_FIT_PARAMS; i++) {
            if (fit_params[i] <= 0.0 || fit_params[i] > 10000)
                err = SPE_FIT_ERR_BAD_FIT;
        }

        /* Make sure gaussian max isn't out of the fitted range */
        if (fit_params[3] > xdata[start_bin + ndata-1]) err = SPE_FIT_ERR_BAD_FIT;
    }
    else {
#ifdef DEBUG
        printf("SPE fit error: singular matrix or no convergence!\r\n");
#endif
        err = SPE_FIT_ERR_NO_CONVERGE;
    }

    return err;
}
