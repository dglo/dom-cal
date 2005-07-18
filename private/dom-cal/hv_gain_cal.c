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

/*---------------------------------------------------------------------------*/

int hv_gain_cal(calib_data *dom_calib) {

    const int cnt = 128;
    int trigger_mask;
    short bias_dac;
    int ch, bin, trig, peak_idx;
    float bin_v, peak_v, vsum;

    /* Which ATWD to use */
    short atwd = GAIN_CAL_ATWD;

    /* Channel readout buffers for each channel and bin */
    /* This test only uses a single ATWD, ch0 and ch1 */
    short channels[2][128];
    
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

    /* Set discriminator */
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, GAIN_CAL_DISC_DAC);

    /* Get bias DAC setting */
    bias_dac = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    float bias_v = biasDAC2V(bias_dac);

    /* Make sure pulser is off */
    hal_FPGA_TEST_disable_pulser();

    /* Select something static into channel 3 */
    halSelectAnalogMuxInput(DOM_HAL_MUX_FLASHER_LED_CURRENT);

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

    /* Give user a final warning */
#ifdef DEBUG
    printf(" *** WARNING: enabling HV in 5 seconds! ***\r\n");
#endif
    halUSleep(5000000);

    /* Turn on high voltage base */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halEnablePMT_HV();
#else
    halPowerUpBase();
    halEnableBaseHV();
#endif

    /* Check to make sure there *is* a HV base by looking at ID */
    /* Avoids running on, say, the sync board */
    const char *hv_id = halHVSerial();
#ifdef DEBUG
    printf("HV ID is %s\r\n", hv_id);
#endif
    if (strcmp(hv_id, "000000000000") == 0) {
#ifdef DEBUG
        printf("All-zero HV ID!  No HV base attached; aborting gain calibration.\r\n");
#endif
        dom_calib->hv_gain_valid = 0;
        dom_calib->num_histos = 0;        
        return 0;
    }

    short hv_idx = 0;
    short hv;

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

        /* baseline should be independent of ATWD bin -- can store in one variable */
        float baseline = 0;

        /* number of 'baseline' readouts flagged as containing wf */
        int wf_bad = 0;

        /* max allowed variance */
        float max_var = MAXIMUM_BASELINE_VARIANCE;

        /* Calculate baseline */
        for (trig=0; trig<BASELINE_TRIG_CNT; trig++) {

            /* Warm up the ATWD */
            prescanATWD(trigger_mask);

            /* read it! */
            hal_FPGA_TEST_trigger_forced(trigger_mask);
            while (!hal_FPGA_TEST_readout_done(trigger_mask));
            if (atwd == 0) {
                hal_FPGA_TEST_readout(channels[0], NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL,
                                      cnt, NULL, 0, trigger_mask);
            }
            else {
                hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                      channels[0], NULL, NULL, NULL,
                                      cnt, NULL, 0, trigger_mask);
            }

            int i;

            /* calibrated waveform */
            float wf[128];

            /* calibrate waveform */
            for (i = 0; i < 128; i++) wf[i] = getCalibV(channels[0][i], *dom_calib, atwd, 0, i, bias_v);

            /* look at the variance for evidence of signal */
            float mean, var;
            meanVarFloat(wf, 128, &mean, &var);

            /* if variance is too large, redo this iteration */
            if (var > max_var) {
                trig--;
                wf_bad++;
                
                /* if we have too many bad 'baseline' readouts, maybe we're too stringent */
                if (wf_bad == BASELINE_TRIG_CNT) {
                    wf_bad = 0;
                    max_var *= 1.2;
                }

                continue;
            }

            /* sum up the ATWD readout */
            for (i = 0; i < 128; i++) baseline += wf[i];
 
        }

        /* get final baseline value */
        baseline /= (128 * BASELINE_TRIG_CNT);

#ifdef DEBUG
        printf("PMT baseline is %f\r\n", baseline);
#endif

        /* Number of points with negative charge */
        int bad_trig = 0;

        for (trig=0; trig<(int)GAIN_CAL_TRIG_CNT; trig++) {
                
            /* Warm up the ATWD */
            prescanATWD(trigger_mask);
            
            /* Discriminator trigger the ATWD */
            hal_FPGA_TEST_trigger_disc(trigger_mask);
            
            /* Wait for done */
            while (!hal_FPGA_TEST_readout_done(trigger_mask));

            /* Read out one waveform from channels 0 and 1 */
            if (atwd == 0) {
                hal_FPGA_TEST_readout(channels[0], channels[1], NULL, NULL, 
                                      NULL, NULL, NULL, NULL,
                                      cnt, NULL, 0, trigger_mask);
            }
            else {
                hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                      channels[0], channels[1], NULL, NULL,
                                      cnt, NULL, 0, trigger_mask);
            }

            /* Make sure we aren't in danger of saturating channel 0 */            
            /* If so, switch to channel 1 */
            ch = 0;
            for (bin=0; bin<cnt; bin++) {
                if (channels[0][bin] > 800) {
                    ch = 1;
                    break;
                }
            }

            /* Find the peak */
            peak_idx = 0;
            if (atwd == 0) {
                peak_v = (float)channels[ch][0] * dom_calib->atwd0_gain_calib[ch][0].slope
                    + dom_calib->atwd0_gain_calib[ch][0].y_intercept;
            }
            else {
                peak_v = (float)channels[ch][0] * dom_calib->atwd1_gain_calib[ch][0].slope
                    + dom_calib->atwd1_gain_calib[ch][0].y_intercept;
            }

            for (bin=96; bin<cnt; bin++) {

                /* Use calibration to convert to V */
                /* Don't need to subtract out bias or correct for amplification to find */
                /* peak location -- but without correction, it is really a minimum */
                if (atwd == 0) {
                    bin_v = (float)channels[ch][bin] * dom_calib->atwd0_gain_calib[ch][bin].slope
                        + dom_calib->atwd0_gain_calib[ch][bin].y_intercept;
                }
                else {
                    bin_v = (float)channels[ch][bin] * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept;
                }

                if (bin_v < peak_v) {
                    peak_idx = bin;
                    peak_v = bin_v;
                }
            }

            /* Now integrate around the peak to get the charge */
            /* FIX ME: increase sampling speed? */
            /* FIX ME: use time window instead? */
            int int_min, int_max;
            int_min = (peak_idx - INT_WIN_MIN >= 0) ? peak_idx - INT_WIN_MIN : 0;
            int_max = (peak_idx + INT_WIN_MAX <= cnt-1) ? peak_idx + INT_WIN_MAX : cnt-1;
            vsum = 0;

            for (bin = int_min; bin <= int_max; bin++)
                vsum += (getCalibV(channels[ch][bin], *dom_calib, atwd, ch, bin, bias_v) - baseline);

            /* True charge, in pC = 1/R_ohm * sum(V) * 1e12 / (freq_mhz * 1e6) */
            /* FE sees a 50 Ohm load */
            charges[trig] = 0.02 * 1e6 * vsum / freq;
 
            if (charges[trig] < 0) {
                trig--;
                if (++bad_trig > GAIN_CAL_TRIG_CNT) break;
            }

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
	   
        int hist_max = ceil(0.75*pow(10.0, 6.37*log10(hv*2)-21.0));
        int hbin;

        /* Initialize histogram */
        for(hbin=0; hbin < GAIN_CAL_BINS; hbin++) {
            hist_x[hv_idx][hbin] = (float)hbin * hist_max / GAIN_CAL_BINS;
            hist_y[hv_idx][hbin] = 0.0;
        }

        /* Fill histogram -- histogram minimum is 0.0 */
        int hist_under, hist_over;
        hist_under = hist_over = 0;
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

        float valley_x, valley_y, pv_ratio;
        /* Find valley */
        int val = spe_find_valley(fit_params[hv_idx], &valley_x, &valley_y);
        if (val == 0) {

            pv_ratio = fit_params[hv_idx][2] / valley_y;
#ifdef DEBUG
            printf("Valley located at %.6g, %.6g: PV = %.2g\r\n", valley_x, valley_y, pv_ratio);
#endif
            
            /* If PV is too high, we don't have true peak and valley */
            if ((pv_ratio > 0.0) && (pv_ratio < GAIN_CAL_MAX_SANE_PV)) {

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
