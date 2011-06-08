/*
 * Prototypes for fpga configuration 
 */

int is_domapp_loaded(void);
int load_domapp_fpga(void);
int fpga_config(int *p, int nbytes);
void gunzip(uLong srcLen, Bytef *src, uLong *zLen, Bytef **zDest);
