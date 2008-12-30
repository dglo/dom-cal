/*
 * pmt_discriminator_cal
 *
 * Shape of pulser waveform is different than that of PMT;
 * thus, thresholds from discriminator_cal do not correspond
 * to the true discriminator threshold observed in data taking.
 * This routine uses PMT pulses to calibrate the discriminator.
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "hv_gain_cal.h"
#include "calUtils.h"
#include "pmt_discriminator_cal.h"

int pmt_discriminator_cal(calib_data *dom_calib) {

    short hv_idx, hv, cal_cnt;
    int i, count0, count1;
    float charge_thresh[GAIN_CAL_HV_CNT], disc_dac_final[GAIN_CAL_HV_CNT];
    float qmean, dfrac, integral, sum, frac_bin;

    cal_cnt = 0;

    /* Calibration isn't valid until I say so */
    dom_calib->pmt_disc_calib_valid = 0;

    /* Turn on high voltage base */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halEnablePMT_HV();
#else
    halPowerUpBase();
    halEnableBaseHV();
#endif

    /* Check to make sure there *is* a HV base by looking at ID */
    /* Avoids running on, say, the sync board */
    if (!checkHVBase()) {
#ifdef DEBUG
        printf("All-zero HV ID!  No HV base attached;");
        printf(" aborting PMT discriminator calibration.\r\n");
#endif
        return 0;
    }

    // Calculate appropriate voltage points for calibration
    // based on which have histograms from the gain calibration
    int num_filled_pts = 0;
    int num_filled_pts12 = 0;
    for (i = 0; i < GAIN_CAL_HV_CNT; i++) {
        if (dom_calib->histogram_data[i].is_filled) {
            num_filled_pts++;
            if (GAIN_CAL_HV_LOW + GAIN_CAL_HV_INC*i > 1200) num_filled_pts12++;
        }
    }
    
    // If we have 6 points above 1200V, start there, otherwise, start at
    // GAIN_CAL_HV_LOW
    int hv_idx_start;
    if (num_filled_pts12 >= 6) hv_idx_start =
                       (int)floor((1200 - GAIN_CAL_HV_LOW)/GAIN_CAL_HV_INC);
    else hv_idx_start = 0;

    /* Loop over HV settings */
    for (hv_idx = hv_idx_start; hv_idx < GAIN_CAL_HV_CNT; hv_idx++) {

        /* Stop when we have enough points */
        if (cal_cnt >= 6) break;

        /* Set high voltage and give it time to stabilize */
        hv = (hv_idx * GAIN_CAL_HV_INC) + GAIN_CAL_HV_LOW;

        /* Check if current HV is below maximum allowed HV */
        if (hv > dom_calib->max_hv) continue;
        
        /* Check if we have a charge histogram for this voltage */
        if (hv_idx > dom_calib->num_histos || 
                       !dom_calib->histogram_data[hv_idx].is_filled) continue;

#ifdef DEBUG
        printf(" Setting HV to %d V\r\n", hv);
#endif

        /* Set discriminator */
        /* Three different discriminator settings based on HV setting */
        int disc_dac;
        if (hv_idx < 4) disc_dac = getDiscDAC(GAIN_CAL_PC_LOW, *dom_calib);
        else if (hv_idx < 8) disc_dac = getDiscDAC(GAIN_CAL_PC_MED, *dom_calib);
        else disc_dac = getDiscDAC(GAIN_CAL_PC_HIGH, *dom_calib);
        halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, disc_dac);
#ifdef DEBUG
        printf("Using disc DAC %d\n", disc_dac);
#endif

        // Set HV
        halWriteActiveBaseDAC(hv * 2);
        halUSleep(5000000);

        /* Measure rate */
        count0 = count1 = 0;
        for (i = 0; i < PMT_DISC_CAL_NOISE_CNT; i++) {
            halUSleep(101000);
            count0 += hal_FPGA_TEST_get_spe_rate();
        }

        // Increase disc, remeasure rate.  Try to set discrminator at center
        // of gaussian.  First, solve for Q_mean(HV):
        qmean = pow(10, dom_calib->hv_gain_calib.y_intercept *
                        log10(hv)*dom_calib->hv_gain_calib.slope)*Q_E;

        // Nominally, discriminator set with pulser calibration is factor
        // ~1.4 too high.  Correct for this factor
        qmean /= VQ_FUDGE_FACTOR;

        // Calculate new discriminator DAC setting
        disc_dac_final[cal_cnt] = 
                  floor((qmean-dom_calib->spe_disc_calib.y_intercept)/
                                           dom_calib->spe_disc_calib.slope);

        // Reset DAC, count again
        halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH,
                                    (short)disc_dac_final[cal_cnt]);
#ifdef DEBUG
        printf("Using disc DAC %d\n", disc_dac);
#endif
        halUSleep(5000000);
        for (i = 0; i < PMT_DISC_CAL_NOISE_CNT; i++) {
            halUSleep(101000);
            count1 += hal_FPGA_TEST_get_spe_rate();
        }

        // Calculate fraction of noise remaining, integrate histogram
        dfrac = 1.*count1/count0;
        integral = 0.;
        integral += dom_calib->histogram_data[hv_idx].underflow;
        integral += dom_calib->histogram_data[hv_idx].overflow;
        for (i = 0; i < dom_calib->histogram_data[hv_idx].bin_count; i++) {
          integral += dom_calib->histogram_data[hv_idx].y_data[i];
        }
#ifdef DEBUG
        printf("Iteration: %d  Noise Rate Fraction: %f\n", cal_cnt, dfrac);
#endif
        dfrac *= integral;

        // Integrate backwards until we reach the new discriminator fraction
        sum = 0.;
        sum += dom_calib->histogram_data[hv_idx].overflow;
        for (i = dom_calib->histogram_data[hv_idx].bin_count -1; i >= 0; i--) {
          sum += dom_calib->histogram_data[hv_idx].y_data[i];
          if (sum > dfrac) break;
        }

        // Interpolate inside the final bin
        frac_bin = (sum-dfrac)/dom_calib->histogram_data[hv_idx].y_data[i];
        if (i == dom_calib->histogram_data[hv_idx].bin_count - 1) {
          charge_thresh[cal_cnt] = dom_calib->histogram_data[hv_idx].x_data[i];
        } else {
          charge_thresh[cal_cnt++] = 
                 frac_bin*dom_calib->histogram_data[hv_idx].x_data[i+1] +
                    (1.-frac_bin)*dom_calib->histogram_data[hv_idx].x_data[i];
        }
    } // End HV loop

    // Do fit if possible
    if (cal_cnt >= 2) {
      linear_fit temp_fit;
      linearFitFloat(disc_dac_final, charge_thresh, cal_cnt, &temp_fit);
      // FIX ME
      printf("Old: a: %g b: %g  New: a: %g b: %g\n", dom_calib->spe_disc_calib.slope, dom_calib->spe_disc_calib.y_intercept, temp_fit.slope, temp_fit.y_intercept);
    }

    return 0;
}
