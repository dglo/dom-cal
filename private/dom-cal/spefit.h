/* Fraction of hits in first real bin */
#define SPE_FIT_NOISE_FRACT     0.0075

/* Fraction of hits to consider essentially zero */
#define SPE_FIT_ZERO_FRACT      0.0004

/* Fraction of data to chop off high end of SPE peak -- eliminate non-gaussian component */
#define SPE_BAD_TAIL_FRACTION   0.04

/* Fraction of gaussian peak position for allowable negative valley */
#define SPE_FIT_VALLEY_PEAK_FRACT  0.075

/* Fit information */
#define SPE_FIT_PARAMS              5
#define SPE_FIT_MAX_ITER           75
#define SPE_FIT_CHISQ_ABS_DONE   0.01
#define SPE_FIT_CHISQ_PCT_DONE  0.001

/* Largest gaussian width parameter (most narrow) */
#define SPE_FIT_MAX_GAUSS_WIDTH  1000

/* Newton-Raphson valley finder parameters */
#define SPE_FIT_NR_MAX_ERR     0.01
#define SPE_FIT_NR_MAX_ITER      20

/* Number of profiles to try */
#define SPE_FIT_PROFILE_CNT    3

/* LM fit errors */
#define SPE_FIT_ERR_NO_CONVERGE       1
#define SPE_FIT_ERR_BAD_FIT           2
#define SPE_FIT_ERR_SINGULAR          3
#define SPE_FIT_ERR_EMPTY_HIST        4

/* Valley finder errors */
#define SPE_FIT_ERR_NR_NO_CONVERGE    5
#define SPE_FIT_ERR_NR_BAD_X          6

int spe_fit(float *xdata, float *ydata, int pts,
                         float *fit_params, int num_samples, int profile );
int spe_find_valley(float *a, float *valley_x, float *valley_y);
