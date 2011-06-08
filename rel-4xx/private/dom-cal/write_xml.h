/*
 * write_xml prototypes
 */

/* Buffer for zlib compression */
#define ZBUFSIZE                             8000

int write_xml(calib_data *dom_calib, int throttle, int compress);

void write_linear_fit(linear_fit fit, int compress, z_stream *zs);
void write_quadratic_fit(quadratic_fit fit, int compress, z_stream *zs);
void write_histogram(hv_histogram histo, int compress, z_stream *zs);

int zprintstr(char *str, int compress, z_stream *zs, int last);

