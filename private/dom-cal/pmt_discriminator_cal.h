/*
 * pmt_discriminator_cal header file
 */

// Expected ratio V/Q pulser to V/Q PMT
#define VQ_FUDGE_FACTOR 1.4

// Number of 100 msec noise iterations to do
#define PMT_DISC_CAL_NOISE_CNT 200

/* Prototypes */
int pmt_discriminator_cal(calib_data *dom_calib);
