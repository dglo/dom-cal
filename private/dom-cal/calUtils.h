/* 
 * Prototypes for calUtils.c
 */
float getCalibV(short val, calib_data dom_calib, int atwd, int ch, int bin, float bias_v);
float getCalibFreq(int atwd, calib_data dom_calib, short sampling_dac);
short getFreqCalib(int atwd, calib_data dom_calib, float sampling_freq);
float temp2K(short temp);
float pulserDAC2Q(int pulser_dac);
float discDAC2V(int disc_dac, int bias_dac);
int discV2DAC(float disc_v, int bias_dac);
float biasDAC2V(int val);
float getFEImpedance(short toroid_type);
short getTorroidType(float fe_impedance);
void prescanATWD(unsigned int trigger_mask);
void meanVarFloat(float *x, int pts, float *mean, float *var);
void linearFitFloat(float *x, float *y, int pts, linear_fit *fit);
void quadraticFitFloat(float *x, float *y, int pts, quadratic_fit *fit);
int checkHVBase();
int getDiscDAC(float charge_threshold, calib_data dom_calib);
void refineLinearFit(float *x, float *y, int *vld_cnt, char *vld, 
                     linear_fit *fit, float minR2, int minPts);
void sort(float *data, int cnt);

