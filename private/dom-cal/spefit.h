/* Fit information -- number of parameters */
#define SPE_FIT_PARAMS            5
#define SPE_FIT_MAX_ITER         20
#define SPE_FIT_CHISQ_DONE     0.01

int spe_fit(float *xdata, float *ydata, int pts, float *fit_params);
