/*
 * lmfit.c
 *
 * Implements the Levenberg-Marquardt nonlinear regression
 * algorithm.
 *
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "lmfit.h"

/*---------------------------------------------------------------------------*/
/*
 * vector
 *
 * Allocate a float vector of $size bins
 */

float *vector( int size ) {
    
    float *v;
    
    /* Allocate vector */
    v = ( float* )malloc( ( size_t )( ( size )*sizeof( float ) ) );
    
    return v;

}

/*---------------------------------------------------------------------------*/
/*
 * int_vector
 *
 * Allocate an int vector of $size bins
 */

int *int_vector( int size ) {

    int *v;

    /* Allocate vector */
    v = ( int* )malloc( ( size_t )( ( size )*sizeof( int ) ) );

    return v;

}

/*---------------------------------------------------------------------------*/
/*
 * matrix
 *
 * Allocate a float matrix of $rows rows and $cols columns
 */

float **matrix( int rows, int cols ) {

    float **m;
    
    /* Allocate pointers to rows */
    m = ( float** )malloc( ( size_t )( ( rows )*sizeof( float* ) ) );
    
    /* Allocate matrix */
    int i;
    for ( i = 0; i < rows; i++ ) {
        m[i] = ( float* )malloc( ( size_t )( ( cols )*sizeof( float ) ) );
    }

    return m;

}

/*---------------------------------------------------------------------------*/
/*
 * free_vector
 *
 * Free allocated vector
 */

void free_vector( float *v ) {
    free( v );
}

/*---------------------------------------------------------------------------*/
/*
 * free_int_vector
 *
 * Free allocated vector
 */

void free_int_vector( int *v ) {
    free( v );
}


/*---------------------------------------------------------------------------*/
/*
 * free_matrix
 *
 * Free allocated matrix
 */

void free_matrix( float **m, int nrows ) {
    int i = 0;
    for ( i = 0; i < nrows; i++ ) {
        free( m[i] );
    }
    free( m );
}

/*---------------------------------------------------------------------------*/

void swap( float *i, float *j ) {

    float temp;
    temp = *i;
    *i = *j;
    *j = temp;

}

/*---------------------------------------------------------------------------*/
/*
 * gauss_jordan
 *
 * Gauss-Jordan elimination
 */

int gauss_jordan( float **matrix, int n, float **solution_set, int m ) {
    
    int piv_row, piv_col;
    int i, j, k;

    int *piv_row_index = int_vector( n );
    int *piv_col_index = int_vector( n );
    int *is_piv = int_vector( n );

    piv_row = piv_col = 0;

    for ( i = 0; i < n; i++ ) {
        is_piv[i] = 0;
    }

    float max_element;
    for ( i = 0; i < n; i++ ) {
        max_element = 0.0;
        for ( j = 0; j < n; j++ ) {
            if ( is_piv[j] != 1 ) {
                for ( k = 0; k < n; k++ ) {
                    if ( is_piv[k] == 0 ) {
                        if ( fabs( matrix[j][k] ) >= max_element ) {
                            max_element = fabs( matrix[j][k] );
                            piv_row = j;
                            piv_col = k;
                        }
                    } else if ( is_piv[k] > 1 ) {
                        return ESINGULAR_MATRIX;
                    }
                }
            }
        }
        ( is_piv[piv_col] )++;

        if ( piv_row != piv_col ) {
            for ( j = 0; j < n; j++ ) {
                swap( &matrix[piv_row][j], &matrix[piv_col][j] );
            }
            for ( j = 0; j < m; j++ ) {
                swap( &solution_set[piv_row][j], &solution_set[piv_col][j] );
            }
        }

        piv_row_index[i] = piv_row;
        piv_col_index[i] = piv_col;

        if ( matrix[piv_col][piv_col] == 0.0 ) {
            return ESINGULAR_MATRIX;
        }

        float inverse = 1.0 / matrix[piv_col][piv_col];
        matrix[piv_col][piv_col] = 1.0;

        for ( j = 0; j < n; j++ ) {
            matrix[piv_col][j] *= inverse;
        }

        for ( j = 0; j < m; j++ ) {
            solution_set[piv_col][j] *= inverse;
        }

        float reduc_factor;

        for ( j = 0; j < n; j++ ) {
            if ( j != piv_col ) {
                reduc_factor = matrix[j][piv_col];
                matrix[j][piv_col] = 0.0;
                for ( k = 0; k < n; k++ ) {
                    matrix[j][k] -= matrix[piv_col][k]*reduc_factor;
                }
                for ( k = 0; k < m; k++ ) {
                    solution_set[j][k] -= solution_set[piv_col][k]*reduc_factor;
                }
            }
        }
    }

    for ( i = n-1; i >=0; i-- ) {
        if ( piv_row_index[i] != piv_col_index[i] ) {
            for ( j = 0; j < n; j++ ) {
                swap( &matrix[j][piv_row_index[i]], 
                                  &matrix[j][piv_col_index[i]] );
            }
        }
    }
    
    free_int_vector( piv_row_index );
    free_int_vector( piv_col_index );
    free_int_vector( is_piv );
    
    return 0;
}

/*---------------------------------------------------------------------------*/
/*
 * covariance_sort
 *
 * Sort covariance matrix
 */
 
void covariance_sort( float **covariance_matrix, int nparams ) {

    int i, j;
    
    for ( i = nparams - 1; i >= 0; i-- ) {
        for ( j = 0; j < nparams; j++ ) {
            swap( &covariance_matrix[j][i], &covariance_matrix[j][i] );
        }
        for ( j = 0; j < nparams; j++ ) {
            swap( &covariance_matrix[i][j], &covariance_matrix[i][j] );
        }
    }
}

/*--------------------------------------------------------------------------*/
/*
 * lm_matrix
 *
 * Using data to fit, calculate matrices and chi-squared used in L-M fit 
 * iterations.
 *
 */
void lm_matrix(float *x, float *y, float *sigma, int ndata, float *a, int nparam,
              float **alpha, float *beta, float *chisq, 
              void (*funcs)(float, float *, float *, float *, int)) {

    int i, j, k;
    float ymod, wt, sigma2i, dy, *dyda;

    dyda = vector(nparam);

    /* Initialize alpha, beta */
    for (j=0; j < nparam; j++) {
        for (k=0; k <= j; k++) 
            alpha[j][k]=0.0;
        beta[j]=0.0;
    }

    *chisq = 0.0;

    /* Summation loop over all data */
    for (i=0; i < ndata; i++) {

        /* Get fit value and derivatives */
        (*funcs)(x[i], a, &ymod, dyda, nparam);

        sigma2i = 1.0/(sigma[i]*sigma[i]);
        dy = y[i]-ymod;

        for (j=0; j < nparam; j++) {
            wt = dyda[j]*sigma2i;

            for (k=0; k <= j; k++) 
                alpha[j][k] += wt*dyda[k];

            beta[j] += dy*wt;
        }
        *chisq += dy*dy*sigma2i;
    }

    /* Copy symmetric matrix indices */
    for (j=1; j < nparam; j++)
        for (k=0; k < j; k++) 
            alpha[k][j] = alpha[j][k];

    free_vector(dyda);    
}

/*--------------------------------------------------------------------------*/
/*
 * lmfit
 *
 * Primary Levenberg-Marquart fit routine.  Takes arrays of floats as input 
 * data to fit, as well as errors on the y-value at each point.  The function
 * pointer funcs points to the model for the fit -- values for the function as 
 * well as the Jacobian with respect to the fit parameters are returned.
 *
 * The parameter lambda indicates the weight on the matrix diagonals, but also
 * controls aspects of lmfit -- a negative value indicates that this is the first
 * iteration and initializes the necessary structures.  A zero value indicates
 * the fit has achieved the necessary convergence, and returns the covariance
 * and curvature matrices.  
 *
 */
int lmfit(float *x, float *y, float *sigma, int ndata, float *a, int nparam, 
          float **covar, float **alpha, float *chisq, 
          void (*funcs)(float, float *, float *, float *, int), float *lambda) {

    int j,k;
    static float ochisq,*atry,*beta,*da,**oneda;

    /* Initialization */
    if (*lambda < 0.0) {

        atry = vector(nparam);
        beta = vector(nparam);
        da = vector(nparam);

        oneda = matrix(nparam,1);
        *lambda=0.001;

        lm_matrix(x,y,sigma,ndata,a,nparam,alpha,beta,chisq,funcs);

        ochisq = *chisq;

        for (j = 0; j < nparam; j++)
            atry[j] = a[j];
    }
        
    /* Augment diagonal elements */
    for (j=0; j < nparam; j++) {

        for (k=0; k < nparam; k++)
            covar[j][k] = alpha[j][k];

        covar[j][j] = alpha[j][j]*(1.0 + (*lambda));
        oneda[j][0] = beta[j];
    }

    int err = gauss_jordan(covar, nparam, oneda, 1);
    if ((err) && (*lambda != 0.0))
        return err;
    
    for (j = 0; j < nparam; j++)
        da[j] = oneda[j][0];

    /* Evaluate covariance matrix once converged */
    if (*lambda == 0.0) {
        covariance_sort(covar, nparam);
        free_matrix(oneda, nparam);
        free_vector(da);
        free_vector(beta);
        free_vector(atry);
        return 0;
    }

    /* New values for parameters */
    for (j = 0; j < nparam; j++)
        atry[j] = a[j] + da[j];

    lm_matrix(x,y,sigma,ndata,atry,nparam,covar,da,chisq,funcs);

    /* Did the trial succeed? */
    if (*chisq < ochisq) {

        /* Accept new solution */        
        *lambda *= 0.1;
        ochisq = *chisq;

        for (j=0; j < nparam; j++) {

            for(k=0; k < nparam; k++)
                alpha[j][k] = covar[j][k];

            beta[j] = da[j];
            a[j] = atry[j];
        }
    }
    else {
        *lambda *= 10.0;
        *chisq = ochisq;       
    }

    return 0;
}
