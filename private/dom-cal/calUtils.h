/* 
 * Prototypes for calUtils.c
 */

float biasDAC2V(int val);
void meanVarFloat(float *x, int pts, float *mean, float *var);
void linearFitFloat(float *x, float *y, int pts, float *m, float *b, float *rsqr);
void prescanATWD(unsigned int trigger_mask);
