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
	    
    /* Exponential amplitude */
    params[0] = y[0];
                                                                                
    /* Gaussian amplitude */
    params[2] = y[0];
                                                                                
    /* Find mean and variance */
    float sum = 0;
    int i;
    for ( i = 0; i < num; i++ ) {
        /* we know y values are actually integers */
        sum += y[i];
    }
    
    float *xvals = (float *) malloc((int)sum * sizeof(float));

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
    x = 0.0;
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

    /* Data vectors */
    float *sigma = vector(pts);
    float *x = vector(pts);
    float *y = vector(pts);

    /* Working matrices */
    float **alpha = matrix(SPE_FIT_PARAMS, SPE_FIT_PARAMS);
    float **covar = matrix(SPE_FIT_PARAMS, SPE_FIT_PARAMS);
    
    /* Chi squared */
    float chisq = 0.0;

    /* Must be negative for first iteration */
    float lambda = -1.0;
    
    int found = 0;
    ndata = 0;
    for (i = 0; i < pts; i++) {
        /* Suppress initial zeros */
        if ((found) || (ydata[i] > 0)) {
            found = 1;

            x[ndata] = xdata[i];
            y[ndata] = ydata[i];

            /* Std. dev. estimate for each point */
            /* sigma[ndata] = sqrt(ydata[i]); */
            /* Shouldn't be < 1.0 */
            /* sigma[ndata] = (sigma[ndata] < 1.0) ? 1.0 : sigma[ndata]; */

            /* TEMP */
            sigma[ndata] = 0.1;

#ifdef DEBUG
            printf ("data: %g %g %g\n", x[ndata], y[ndata], sigma[ndata]);
#endif
            ndata++;
        }
#ifdef DEBUG
        else {
            printf("Dropping zero in bin %d\n", i);
        }
#endif
    }

    /* Check to make sure histogram isn't empty */
    if (ndata == 0) {
#ifdef DEBUG
        printf("SPE fit error: empty histogram!\r\n");        
#endif
        return SPE_FIT_ERR_EMPTY_HIST;
    }
    
    /* Find a maximal bin with > 1.5% of total hits */
    int start_bin = 0;
    while ( y[start_bin] < ( 0.015 * num_samples ) &&
              start_bin < pts - 1 && y[start_bin] < y[start_bin + 1] ) {
        start_bin++;
    }

    /* if start_bin > pts / 2, we're better off starting at 0 */
    if ( start_bin > pts / 2 ) {
        start_bin = 0;
    }

    /* Get starting fit parameters */
    get_fit_initialization( &x[start_bin], &y[start_bin],
                                        ndata - start_bin, fit_params);

    /* Print values */
#ifdef DEBUG
    printf("Starting parameter values:\n");
    for (i = 0; i < SPE_FIT_PARAMS; i++)
        printf(" a[%d] = %g\n", i, fit_params[i]);
#endif

    /* Fit loop */
    int done = 0;
    int converged = 0;
    float old_chisq = chisq;
    float del_chisq;

    while (!done) {

        printf("Iteration %d\n", iter);

        /* Loop is done when the max number of iterations is reached */
        /* or when chi-squared has decreased, but only by a small amount */
        /* Decrease can be small absolutely or as a percentage */
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

        /* Do an iteration */
        if (lmfit(&x[start_bin], &y[start_bin], sigma, ndata - start_bin,
                                         fit_params, SPE_FIT_PARAMS, covar,
                                         alpha, &chisq, f_spe, &lambda)) {
            done = 1;
            err = SPE_FIT_ERR_SINGULAR;
#ifdef DEBUG
            printf("SPE fit error: singular matrix\r\n");
#endif
        };

        /* Print values */
#ifdef DEBUG
        printf("Chisq: %.6g\r\n", chisq);
        for (i = 0; i < SPE_FIT_PARAMS; i++)
            printf(" a[%d] = %g\n", i, fit_params[i]);
#endif

    }

    /* Read out covariance matrix */   
    free_vector(x);
    free_vector(y);
    free_vector(sigma);
    free_matrix(alpha, SPE_FIT_PARAMS);
    free_matrix(covar, SPE_FIT_PARAMS);

    /* Check fit for convergence and sanity */
    if (converged) {
#ifdef DEBUG
        printf("SPE fit converged successfully\r\n");
#endif
        /* Check that all fit parameters are positive */
        for (i = 0; i < SPE_FIT_PARAMS; i++) {
            if (fit_params[i] <= 0.0)
                err = SPE_FIT_ERR_BAD_FIT;
        }
    }
    else {
#ifdef DEBUG
        printf("SPE fit error: no convergence after max iterations\r\n");
#endif
        err = SPE_FIT_ERR_NO_CONVERGE;
    }

    return err;
}
