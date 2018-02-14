/*
 * pmt_discriminator_cal header file
 */


#ifdef DOMCAL_SCINT

// Define charge range for calibration
#define PMT_DISC_CAL_Q_LOW 0.16
#define PMT_DISC_CAL_Q_INC 0.1
#define PMT_DISC_CAL_Q_CNT 8

// Define thresholds for charge histograms
#define PMT_DISC_CAL_PC_LOW 0.1
#define PMT_DISC_CAL_PC_MED 0.2

// Minimum noise rate to complete calibration
#define PMT_DISC_CAL_NOISE_MIN  8

#else

// Define charge range for calibration
#define PMT_DISC_CAL_Q_LOW 0.45
#define PMT_DISC_CAL_Q_INC 0.4
#define PMT_DISC_CAL_Q_CNT 8

// Define thresholds for charge histograms
#define PMT_DISC_CAL_PC_LOW 0.14
#define PMT_DISC_CAL_PC_MED 0.25

// Minimum noise rate to complete calibration
#define PMT_DISC_CAL_NOISE_MIN 150

#endif

// Number of 100 msec noise iterations to do
#define PMT_DISC_CAL_NOISE_CNT 200

// Rough ratio V/Q pulser to V/Q PMT
#define VQ_FUDGE_FACTOR 1.3

// Maximum noise rate -- above this indicates an error setting the disc.
#define PMT_DISC_CAL_NOISE_MAX 100000

// Number of charges to histogram
#define PMT_DISC_CAL_HIST_CNT  25000

/* Minimum acceptable R^2 */
#define PMT_DISC_CAL_MIN_R2        0.99

/* Decrease points in fit until you have this many or fewer */
#define PMT_DISC_CAL_MIN_R2_PTS    4

/* Prototypes */
int pmt_discriminator_cal(calib_data *dom_calib);
