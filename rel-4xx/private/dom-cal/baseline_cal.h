
/* Number of pedestal patterns to average */
#define BASELINE_CAL_TRIG_CNT       100

/* Maximum variance of the baseline (for no waveform contamination) */
#define BASELINE_CAL_MAX_VAR     3.0E-6

void getBaseline(calib_data *dom_calib, float max_var, float base[2][3]);
int hv_baseline_cal(calib_data *dom_calib);
int baseline_cal(calib_data *dom_calib);
