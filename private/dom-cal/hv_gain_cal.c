/*
 * hv_gain_cal
 *
 * HV gain calibration routine -- integrates SPE waveforms
 * to create charge histogram, and then fits to find peak and valley.
 * Then performs linear regression on log(HV) vs. log(gain).
 *
 * Iterates over a number of HV settings.
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
#include "spefit.h"
#include "fast_acq.h"

/*---------------------------------------------------------------------------*/

int hv_gain_cal(calib_data *dom_calib) {

    const int cnt = 128;
    int trigger_mask;
    short bias_dac;
    int ch, bin, trig, peak_idx, i;
    float vsum;

    /* Which ATWD to use */
    short atwd = GAIN_CAL_ATWD;

    /* Charge arrays for each waveform */
    float charges[GAIN_CAL_TRIG_CNT];

    /* Charge histograms for each HV */
    float **hist_y = malloc(GAIN_CAL_HV_CNT*sizeof(float*));

    /* Actual x-values for each histogram */
    float **hist_x = malloc(GAIN_CAL_HV_CNT*sizeof(float*));

    /* Fit parameters for each SPE spectrum */
    float **fit_params = malloc(GAIN_CAL_HV_CNT*sizeof(float*));

    /* Log(gain) values for each voltage with a good SPE fit */
    float log_gain[GAIN_CAL_HV_CNT];

    /* Log(HV) settings, for regression */
    float log_hv[GAIN_CAL_HV_CNT];

    /* Number of reasonable P/V histograms obtained */
    int spe_cnt = 0;

    /* Charge histogram data */
    static hv_histogram hv_hist_data[GAIN_CAL_HV_CNT];

#ifdef DEBUG
    printf("Performing HV gain calibration (using ATWD%d)...\r\n", atwd);
#endif

    /* Save DACs that we modify */
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);

    /* Get bias DAC setting */
    bias_dac = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    float bias_vv = biasDAC2V(bias_dac);
    volt_t bias_v = to_volt_t(bias_vv);

    /* Make sure pulser is off */
    hal_FPGA_TEST_disable_pulser();

    /* Disable the analog mux -- it affects the baseline! */
    halDisableAnalogMux();

    /* Trigger one ATWD only */
    trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Get calibrated sampling frequency */
    float freq;
    short samp_dac;
    if (atwd == 0)
        samp_dac = halReadDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS);
    else
        samp_dac = halReadDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);

    freq = getCalibFreq(atwd, *dom_calib, samp_dac);

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
        printf("All-zero HV ID!  No HV base attached; aborting gain calibration.\r\n");
#endif
        dom_calib->hv_gain_valid = 0;
        dom_calib->num_histos = 0;        
        return 0;
    }

    short hv_idx = 0;
    short hv;

    /* Build integer calibration tables */
    integer_calib int_calib;
    build_integer_calib(dom_calib, &int_calib);

    /* Loop over HV settings */
    for (hv_idx = 0; hv_idx < GAIN_CAL_HV_CNT; hv_idx++) {
        
        /* malloc current hv_idx */
        fit_params[hv_idx] = malloc(GAIN_CAL_BINS*sizeof(float));
        hist_x[hv_idx] = malloc(GAIN_CAL_BINS*sizeof(float));
        hist_y[hv_idx] = malloc(GAIN_CAL_BINS*sizeof(float));

        /* Set high voltage and give it time to stabilize */
        hv = (hv_idx * GAIN_CAL_HV_INC) + GAIN_CAL_HV_LOW;      

#ifdef DEBUG
        printf(" Setting HV to %d V\r\n", hv);
#endif

        /* Add hv setting to histogram struct */
        hv_hist_data[hv_idx].voltage = hv;
        hv_hist_data[hv_idx].bin_count = GAIN_CAL_BINS;
        hv_hist_data[hv_idx].convergent = 0;
        hv_hist_data[hv_idx].is_filled = 0;
        hv_hist_data[hv_idx].pv = 0.0;

        /* Set discriminator */
        int disc_dac;
        if (hv_idx < 4) disc_dac = GAIN_CAL_DISC_DAC_LOW;
        else if (hv_idx < 8) disc_dac = GAIN_CAL_DISC_DAC_MED;
        else disc_dac = GAIN_CAL_DISC_DAC_HIGH;
        halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, disc_dac);

        halWriteActiveBaseDAC(hv * 2);
        halUSleep(5000000);

        /* SPE scaler data -- sum for averaging later */
        float noise_sum = 0.0;
        
        /* Get several iterations of noise data */
        int n_idx;
        for (n_idx = 0; n_idx < NOISE_CNT; n_idx++) {
            halUSleep(250000);
            int rate = hal_FPGA_TEST_get_spe_rate() * 10;
#ifdef DEBUG
            printf("Noise iteration %d: noise rate: %d\r\n", n_idx, rate);
#endif
            noise_sum += rate;
        }

        /* calculate average */
        float noise_rate = NOISE_CNT == 0 ? 0.0 : noise_sum / NOISE_CNT;
        hv_hist_data[hv_idx].noise_rate = noise_rate;
#ifdef DEBUG
        printf("Noise rate: %f minimum allowed: %d\r\n", noise_rate, MIN_NOISE);
#endif

        /* if rate is too low, go on to the next voltage */
        if (noise_rate < MIN_NOISE) {
#ifdef DEBUG
            printf("Noise rate too low -- skipping voltage %d\r\n", hv);
#endif
            continue;
        }

        volt_t baseline[2][3];
        for (i = 0; i < 3; i++) {
            baseline[0][i] = to_volt_t(dom_calib->baseline_data[hv_idx].atwd0_hv_baseline[i]);
            baseline[1][i] = to_volt_t(dom_calib->baseline_data[hv_idx].atwd1_hv_baseline[i]);
        }

        /* Number of points with negative charge */
        int bad_trig = 0;

        /* Index, used to determine prescan */
        int idx = 0;

        volt_t vdat[cnt];

        for (trig=0; trig<(int)GAIN_CAL_TRIG_CNT; trig++) {

            fast_acq_wf(vdat, atwd, cnt, GAIN_CAL_START_BIN,
                        trigger_mask, bias_v,
                        &int_calib, baseline, &ch, DISC_TRIGGER, 0, !idx++);

            /* Find the peak */
            peak_idx = 0;
            volt_t min = 0.0;
            for (bin = GAIN_CAL_START_BIN; bin < cnt; bin++) {

                if (bin == GAIN_CAL_START_BIN) {
                    min = vdat[bin];
                    peak_idx = bin;
                } else {
                    if (vdat[bin] < min) {
                        min = vdat[bin];
                        peak_idx = bin;
                    }
                }
            }

            /* Integrate wf */
            int int_min, int_max;
            int_min = (peak_idx - INT_WIN_MIN >= GAIN_CAL_START_BIN) ?
                                         peak_idx - INT_WIN_MIN : GAIN_CAL_START_BIN;
            int_max = (peak_idx + INT_WIN_MAX <= cnt-1) ? peak_idx + INT_WIN_MAX : cnt-1;
            vsum = 0;

            /* Do current integral -- work in front end 50 Ohm load */
            /* to avoid integer overflow */
            for (bin = int_min; bin <= int_max; bin++) vsum += vdat[bin]/50;

            /* True charge, in pC = 1/R_ohm * sum(V) * 1e12 / (freq_mhz * 1e6) */
            /* Need to now divide by amplification factor */
            charges[trig] =  (to_v(vsum) * 1e6 / freq) /
                                          int_calib.amplifier_calib[ch].value;

#ifdef DEBUG
            if (trig%1000 == 0) printf("Got trigger %d\n", trig);      
#endif

        } /* End trigger loop */

        /* Check histogram trigger count -- < GAIN_CAL_TRIG_CNT ==> error, skip voltage */
        if (trig < GAIN_CAL_TRIG_CNT) {
#ifdef DEBUG
            printf("Aborted histogram -- too much bad data -- skipping voltage %d\r\n", hv);
#endif
            continue;
        }
    
        /* histogram has data */
        hv_hist_data[hv_idx].is_filled = 1;

        /* Create histogram of charge values */
        /* Heuristic maximum for histogram */
	   
        float hist_max = 0.75*pow(10.0, 6.37*log10(hv*2)-21.0);
        int hbin, hist_under, hist_over;
        hist_under = 0;
        hist_over = 0;

        /* Re-bin histogram while too many charge points overflow */
        /* 200pC is a sane maximum for any IceCube PMT */
        for (; hist_max < 200.0; hist_max *= 1.5) {

            printf("Trying histogram with maximum %fpC\n", hist_max);

            /* Initialize histogram */
            for(hbin=0; hbin < GAIN_CAL_BINS; hbin++) {
                hist_x[hv_idx][hbin] = (float)hbin * hist_max / GAIN_CAL_BINS;
                hist_y[hv_idx][hbin] = 0.0;
            }
            hist_under = 0;
            hist_over = 0;

            /* Fill histogram -- histogram minimum is 0.0 */
            for(trig=0; trig < GAIN_CAL_TRIG_CNT; trig++) {

                hbin = charges[trig] * GAIN_CAL_BINS / hist_max;
            
                /* Do NOT use an overflow bin; will screw up fit */
                if ((hbin >= 0) && (hbin < GAIN_CAL_BINS))
                    hist_y[hv_idx][hbin] += 1;
                else if (hbin < 0)
                    hist_under++;
                else
                    hist_over++;
            }

            /* We don't need to re-bin if less than 15% is overflow */
            float overflow_ratio = (float)hist_over / GAIN_CAL_TRIG_CNT;
            if (overflow_ratio < 0.15) break;
        }

#ifdef DEBUG
        printf("Negative charge points discarded: %d\r\n", bad_trig); 
        printf("Histogram points out of range: under %d, over %d\r\n", 
               hist_under, hist_over);
        printf("Histogram:\r\n");
        for(hbin=0; hbin < GAIN_CAL_BINS; hbin++) 
            printf("%d %g %g\r\n", hbin, hist_x[hv_idx][hbin], hist_y[hv_idx][hbin]);
#endif

	/* Fit histograms */
	int fiterr;
	fiterr = spe_fit(hist_x[hv_idx], hist_y[hv_idx], GAIN_CAL_BINS, fit_params[hv_idx], GAIN_CAL_TRIG_CNT );

        /* Record fit params */
        hv_hist_data[hv_idx].fit = fit_params[hv_idx];

        /* Record histogram */
        hv_hist_data[hv_idx].x_data = hist_x[hv_idx];
        hv_hist_data[hv_idx].y_data = hist_y[hv_idx];
    /* If no error in fit, record gain and P/V */
    if (!fiterr) {

        float valley_x, valley_y, peak_y, pv_ratio;
        /* Find valley */
        int val = spe_find_valley(fit_params[hv_idx], &valley_x, &valley_y);
        if (val == 0) {

            /* Peak value is Gaussian + exponential at x-value defined */
            /* by Gaussian peak */
            peak_y = fit_params[hv_idx][2] + 
                (fit_params[hv_idx][0] * exp(-1.0 * fit_params[hv_idx][1] * fit_params[hv_idx][3]));

            pv_ratio = peak_y / valley_y;
#ifdef DEBUG
            printf("Full peak (exp + gaussian) = %.6g\r\n", peak_y);
            printf("Valley located at %.6g, %.6g: PV = %.2g\r\n", valley_x, valley_y, pv_ratio);
#endif
            
            /* If PV < 1.1, we have fit problems */
            if (pv_ratio > 1.1) {

                log_hv[spe_cnt] = log10(hv);
                log_gain[spe_cnt] = log10(fit_params[hv_idx][3] / Q_E) - 12.0;

#ifdef DEBUG
                printf("New gain point: log(V) %.6g log(gain) %.6g\r\n", log_hv[spe_cnt], log_gain[spe_cnt]);
#endif         
                /* note fit convergence */
                hv_hist_data[hv_idx].convergent = 1; 
               
                hv_hist_data[hv_idx].pv = pv_ratio;
                
                spe_cnt++;
                
            }
            else {
#ifdef DEBUG
                printf("PV ratio unrealistic: gain point at %d V not used\r\n", hv);
#endif
            }
        }
        else {
#ifdef DEBUG
            printf("SPE valley finder error: gain point at %d V not used\r\n", hv);
            printf("Error is: %d\r\n", val);
#endif
        }
    }
    else {
#ifdef DEBUG
        printf("Bad SPE fit: gain point at %d V not used\r\n", hv);
        printf("Error is: %d\r\n", fiterr);
#endif
    }
    
    } /* End HV loop */

    /* Add histos to calib struct */
    dom_calib->histogram_data = hv_hist_data;

    if (spe_cnt >= 2) {
        /* Fit log(hv) vs. log(gain) */
        linearFitFloat(log_hv, log_gain, spe_cnt, &(dom_calib->hv_gain_calib)); 
        dom_calib->hv_gain_valid = 1;
    }
    else {
#ifdef DEBUG
        printf("Error: too few gain data points to do the regression!\r\n");
#endif
        dom_calib->hv_gain_valid = 0;
    }

    /* Turn off the high voltage */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halDisablePMT_HV();
#else
    halDisableBaseHV();
    halPowerDownBase();
#endif

    /* Put the DACs back to original state */
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);

    /* FIX ME: return real error code */
    return 0;

}
