/* 
 * Prototypes for calUtils.c
 */
float temp2K(short temp);
float biasDAC2V(int val);
void meanVarFloat(float *x, int pts, float *mean, float *var);
void linearFitFloat(float *x, float *y, int pts, linear_fit *fit);
void prescanATWD(unsigned int trigger_mask);
