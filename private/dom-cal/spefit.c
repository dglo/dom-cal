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
    int k;
    for ( k = 0; k < num; k++ ) {
        printf( "Histogram %d %f\n", k, y[k] );
    }
	    
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
int spe_fit(float *xdata, float *ydata, int pts, float *fit_params) {

    int i, ndata;
    int iter = 0;

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
    float alamda = -1.0;
    
    int found = 0;
    ndata = 0;
    for (i = 0; i < pts; i++) {
        /* Suppress initial zeros */
        if ((found) || (y[i] > 0)) {
            found = 1;
            x[ndata] = xdata[i];
            y[ndata] = ydata[i];
            /* FIX ME */
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

    /* Get starting fit parameters */
    get_fit_initialization(x, y, ndata, fit_params);

    /* Print values */
    printf("Starting parameter values:\n");
    for (i = 0; i < SPE_FIT_PARAMS; i++)
        printf(" a[%d] = %g\n", i, fit_params[i]);

    /* Fit loop */
    int done = 0;
    float old_chisq = chisq;
    float del_chisq;

    while (!done) {

        printf("Iteration %d\n", iter);

        /* Loop is done when the max number of iterations is reached */
        /* or when chi-squared has decreased, but only by a small amount */
        iter++;
        del_chisq = old_chisq - chisq;
        done = (iter > SPE_FIT_MAX_ITER) || 
            ((del_chisq > 0) && (del_chisq < SPE_FIT_CHISQ_DONE));
        
        /* Setting to zero on final iteration will read out covariance and curvature */
        if (done)
            alamda = 0.0;

        /* Save chi-squared */
        old_chisq = chisq;

        /* Do an iteration */
        lmfit(x, y, sigma, ndata, fit_params, SPE_FIT_PARAMS, covar, 
	      alpha, &chisq, f_spe, &alamda);

        /* Print values */
        for (i = 0; i < SPE_FIT_PARAMS; i++)
            printf(" a[%d] = %g\n", i, fit_params[i]);

    }

    /* Read out covariance matrix */   
    free_vector(x);
    free_vector(y);
    free_vector(sigma);
    free_matrix(alpha, SPE_FIT_PARAMS);
    free_matrix(covar, SPE_FIT_PARAMS);

    /* FIX ME: check fit for sanity??? */
    return 0;
}
