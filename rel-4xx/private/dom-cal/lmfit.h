/* 
 * Prototypes for fitUtils.c
 */
float *vector( int size );
int *int_vector( int size );
float **matrix( int rows, int cols );
void free_vector( float *v );
void free_int_vector( int *v );
void free_matrix( float **m, int nrows );
int gauss_jordan( float **a, int n, float **b, int m );
void covariance_sort( float **covariance_matrix, int nparams );
int lmfit(float *x, float *y, float *sigma, int ndata, float *a, int nparam, 
          float **covar, float **alpha, float *chisq, 
          void (*funcs)(float, float *, float *, float *, int), float *alamda);

#define ESINGULAR_MATRIX  -1
