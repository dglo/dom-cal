/*
 * write_xml prototypes
 */

unsigned int write_xml(calib_data *dom_calib, int retx);
void write_linear_fit(unsigned int *pCrc, linear_fit fit);
void write_quadratic_fit(unsigned int *pCrc, quadratic_fit fit);
void write_histogram(unsigned int *pCrc, hv_histogram histo);
void crc_printstr(unsigned int *pCrc, char *str);
void crc32(unsigned int *pCrc, unsigned char *b, int n);
void crc32Once(unsigned int *pCrc, unsigned char cval);
