/* 
 * Prototypes for calUtils.c
 */
float getCalibV(short val, calib_data dom_calib, int atwd, int ch, int bin, short bias_dac);
float getCalibFreq(int atwd, calib_data dom_calib, short sampling_dac);
float temp2K(short temp);
float biasDAC2V(int val);
void meanVarFloat(float *x, int pts, float *mean, float *var);
void linearFitFloat(float *x, float *y, int pts, linear_fit *fit);
void prescanATWD(unsigned int trigger_mask);
