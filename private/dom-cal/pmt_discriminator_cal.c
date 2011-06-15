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
#include "baseline_cal.h"
#include "fast_acq.h"
#include "calUtils.h"
#include "pmt_discriminator_cal.h"

int pmt_discriminator_cal(calib_data *dom_calib) {

    short bias_dac, samp_dac, hv, disc_dac, disc_dac_base;
    int trigger_mask, i, qidx, cnt, count0, count1,
                            trig, bin, cidx, ch, peak_idx;
    float charge_thresh[PMT_DISC_CAL_Q_CNT], disc[PMT_DISC_CAL_Q_CNT],
              charges[PMT_DISC_CAL_HIST_CNT], fbaseline[2][3];
    float q, vsum, freq;
    
    volt_t baseline[2][3];
    volt_t wf_out[128];
    const int max = 128;

    /* Save DACs that we modify */
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);

    /* Calibration isn't valid until I say so */
    dom_calib->pmt_disc_calib_valid = 0;
    dom_calib->pmt_disc_calib_num_pts = 0;

    /* Which ATWD to use */
    short atwd = dom_calib->preferred_atwd;

    /* Make sure pulser is off */
    hal_FPGA_TEST_disable_pulser();

    /* Disable the analog mux -- it affects the baseline! */
    halDisableAnalogMux();

    /* Trigger one ATWD only */
    trigger_mask = (atwd == 0) ?
           HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Get calibrated sampling frequency */
    if (atwd == 0) samp_dac = halReadDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS);
    else samp_dac = halReadDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);
    freq = getCalibFreq(atwd, *dom_calib, samp_dac);

    /* Get bias DAC setting */
    bias_dac = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    float bias_vv = biasDAC2V(bias_dac);
    volt_t bias_v = to_volt_t(bias_vv);

    printf("Performing PMT discriminator calibration...\n");

    /* Build integer calibration tables */
    integer_calib int_calib;
    build_integer_calib(dom_calib, &int_calib);

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

    // Check for a valid gain calibration
    if (!dom_calib->hv_gain_valid) {
      printf("No valid gain calibration.  Cannot perform PMT disc. calib.\n");

      /* Restore modified DACs, turn off HV */
      halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
      halDisablePMT_HV();
#else
      halDisableBaseHV();
      halPowerDownBase();
#endif

      return 0;
    }

    cnt = 0;

    /* Loop over target charges */
    for (qidx = 0; qidx < PMT_DISC_CAL_Q_CNT; qidx++) {

        // Current charge
        q = PMT_DISC_CAL_Q_LOW + qidx*PMT_DISC_CAL_Q_INC;
     
        // Calculate appropriate HV to place SPE peak at selected charge
        hv = (int)(pow(10, (log10(q/(Q_E*1e12)) - 
               dom_calib->hv_gain_calib.y_intercept) / 
               dom_calib->hv_gain_calib.slope));

        /* Check if current HV is below maximum allowed HV */
        if (hv > dom_calib->max_hv) break;

        // Account for expected ~30% difference between pulser and PMT
        // charge thresholds.  We want to set discriminator threshold
        // near SPE peak to minimize error.
        q /= VQ_FUDGE_FACTOR;

        // Calculate discriminator threshold.  Quit if out of range.
        disc_dac = getDiscDAC(q, *dom_calib);
        if (disc_dac  > 1023.) break;

        // Select and set discriminator threshold for histogram
        if (qidx < 4) disc_dac_base =
                      getDiscDAC(PMT_DISC_CAL_PC_LOW, *dom_calib);
        else disc_dac_base = getDiscDAC(PMT_DISC_CAL_PC_MED, *dom_calib);
        halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, disc_dac_base);
#ifdef DEBUG
        printf("Iteration %d: Q: %f, Base Disc: %d, Upper Disc %d, HV: %d\n",
               qidx, q, disc_dac_base, disc_dac, hv); 
#endif
        // Set HV
        halWriteActiveBaseDAC(hv * 2);
        halUSleep(5000000);

        // Measure rate
        count0 = count1 = 0;
        for (i = 0; i < PMT_DISC_CAL_NOISE_CNT; i++) {
            halUSleep(101000);
            count0 += hal_FPGA_TEST_get_spe_rate();
        }

        // Quit this iteration if rate is too low
        if (10.*count0/PMT_DISC_CAL_NOISE_CNT < PMT_DISC_CAL_NOISE_MIN) {
          printf("Noise rate %f too low.  Skipping.\n",
                             10.*count0/PMT_DISC_CAL_NOISE_CNT);
          continue;
        } else if (10.*count0/PMT_DISC_CAL_NOISE_CNT > PMT_DISC_CAL_NOISE_MAX) {
          printf("Noise rate %f too high.  Skipping.\n",
                             10.*count0/PMT_DISC_CAL_NOISE_CNT);
          continue;
        }
        printf("Noise Rate: %f.  Obtaining charge histogram.\n",
                                        10.*count0/PMT_DISC_CAL_NOISE_CNT);

        // Do charge histogram -- first get baseline
        getBaseline(dom_calib, 1., fbaseline);

        // Convert baseline to fast_acq format
        for (i = 0; i < 3; i++) {
            baseline[0][i] = to_volt_t(fbaseline[0][i]);
            baseline[1][i] = to_volt_t(fbaseline[1][i]);
        }

        // Get charge histogram
        for (trig=0; trig<(int)PMT_DISC_CAL_HIST_CNT; trig++) {

            fast_acq_wf(wf_out, atwd, max, GAIN_CAL_START_BIN,
                        trigger_mask, bias_v,
                        &int_calib, baseline, &ch, DISC_TRIGGER, 0, !trig);

            /* Find the peak */
            peak_idx = 0;
            volt_t min = 0;
            for (bin = GAIN_CAL_START_BIN; bin < max; bin++) {
                if (bin == GAIN_CAL_START_BIN) {
                    min = wf_out[bin];
                    peak_idx = bin;
                } else {
                    if (wf_out[bin] < min) {
                        min = wf_out[bin];
                        peak_idx = bin;
                    }
                }
            }

            /* Integrate wf */
            int int_min, int_max;
            int_min = (peak_idx - INT_WIN_MIN >= GAIN_CAL_START_BIN) ?
                           peak_idx - INT_WIN_MIN : GAIN_CAL_START_BIN;
            int_max = (peak_idx + INT_WIN_MAX <= max-1) ?
                                       peak_idx + INT_WIN_MAX : max-1;

            /* Switch to floating point */
            /* Do current integral -- work in front end load */
            vsum = 0.;
            for (bin = int_min; bin <= int_max; bin++)
                             vsum += wf_out[bin]/dom_calib->fe_impedance;

            /* True charge, in pC = 1/R_ohm*sum(V)*1e12 / (freq_mhz*1e6) */
            /* Need to now divide by amplification factor */
            charges[trig] =  (to_v(vsum) * 1e6 / freq) /
                                          int_calib.amplifier_calib[ch].value;

        } /* End trigger loop */

        // Reset DAC, count again
        halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, disc_dac);
        halUSleep(5000000);
        for (i = 0; i < PMT_DISC_CAL_NOISE_CNT; i++) {
            halUSleep(101000);
            count1 += hal_FPGA_TEST_get_spe_rate();
        }

        // Calculate new charge threshold -- first sort the charge data
        sort(charges, PMT_DISC_CAL_HIST_CNT);

        // Calculate index of charge nearest to discriminator threshold
        cidx = (int)(PMT_DISC_CAL_HIST_CNT*(1.-1.*count1/count0));
        if (cidx >= PMT_DISC_CAL_HIST_CNT) cidx = PMT_DISC_CAL_HIST_CNT - 1;
        if (cidx < 0) cidx = 0;
        charge_thresh[cnt] = charges[cidx];
        disc[cnt++] = disc_dac;

#ifdef DEBUG
        printf("New calibration point: Disc DAC: %d Old Threshold: %fpC",
           (short)disc_dac, q);
        printf(" New Threshold: %fpC\n", charge_thresh[cnt-1]);
#endif
    } // End HV loop

    dom_calib->pmt_disc_calib_num_pts = cnt;

    // Do fit if possible
    if (cnt >= 2) {
      linearFitFloat(disc, charge_thresh, cnt, &(dom_calib->pmt_disc_calib));

      /* Check for bad R^2 and refine fit if necessary */
      int origPts = cnt;
      char vld[cnt];
      refineLinearFit(disc, charge_thresh, &cnt, vld,
             &(dom_calib->pmt_disc_calib), PMT_DISC_CAL_MIN_R2,
             PMT_DISC_CAL_MIN_R2_PTS);
      
      /* Report discarded fits as bad */
      for (i = 0; i < origPts; i++) {
          if (!vld[i]) printf("Discarded point %d\n", i);
      }
      dom_calib->pmt_disc_calib_valid = 1;
      dom_calib->pmt_disc_calib_num_pts = cnt;
#ifdef DEBUG
      printf("Old fit: a: %g b: %g r2: %g  New fit: a: %g b: %g r2: %g\n",
        dom_calib->spe_disc_calib.slope, dom_calib->spe_disc_calib.y_intercept,
        dom_calib->spe_disc_calib.r_squared, dom_calib->pmt_disc_calib.slope,
        dom_calib->pmt_disc_calib.y_intercept,
        dom_calib->pmt_disc_calib.r_squared);
#endif
    }

    /* Restore modified DACs, turn off HV */
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halDisablePMT_HV();
#else
    halDisableBaseHV();
    halPowerDownBase();
#endif


    return 0;
}
