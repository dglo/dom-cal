/* 
 * Prototypes for calUtils.c
 */
float getCalibV(short val, calib_data dom_calib, int atwd, int ch, int bin, float bias_v);
float getCalibFreq(int atwd, calib_data dom_calib, short sampling_dac);
float temp2K(short temp);
float pulserDAC2Q(int pulser_dac);
float discDAC2V(int disc_dac, int bias_dac);
int discV2DAC(float disc_v, int bias_dac);
float biasDAC2V(int val);
void prescanATWD(unsigned int trigger_mask);
void meanVarFloat(float *x, int pts, float *mean, float *var);
void linearFitFloat(float *x, float *y, int pts, linear_fit *fit);
void quadraticFitFloat(float *x, float *y, int pts, quadratic_fit *fit);
int checkHVBase();
