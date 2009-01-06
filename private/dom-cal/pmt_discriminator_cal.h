/*
 * pmt_discriminator_cal header file
 */

// Define charge range for calibration
#define PMT_DISC_CAL_Q_LOW 0.4
#define PMT_DISC_CAL_Q_INC 0.4
#define PMT_DISC_CAL_Q_CNT 8

// Number of 100 msec noise iterations to do
#define PMT_DISC_CAL_NOISE_CNT 200

// Rough ratio V/Q pulser to V/Q PMT
#define VQ_FUDGE_FACTOR 1.3

// Minimum noise rate to complete calibration
#define PMT_DISC_CAL_NOISE_MIN 150

// Number of charges to histogram
#define PMT_DISC_CAL_HIST_CNT  25000

/* Prototypes */
int pmt_discriminator_cal(calib_data *dom_calib);
