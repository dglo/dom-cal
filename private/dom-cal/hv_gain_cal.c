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

    /* Channel readout buffers for each channel and bin */
    /* This test only uses ATWD A, ch0 and ch1 */
    short channels[2][128];
    
    /* Charge arrays for each waveform */
    float charges[GAIN_CAL_TRIG_CNT];

    /* Charge histograms for each HV */
    float hist_y[GAIN_CAL_HV_CNT][GAIN_CAL_BINS];

    /* Actual x-values for each histogram */
    float hist_x[GAIN_CAL_HV_CNT][GAIN_CAL_BINS];

    /* Fit parameters for each SPE spectrum */
    float fit_params[GAIN_CAL_HV_CNT][SPE_FIT_PARAMS];

    /* Log(gain) values for each voltage with a good SPE fit */
    float log_gain[GAIN_CAL_HV_CNT];

    /* Log(HV) settings, for regression */
    float log_hv[GAIN_CAL_HV_CNT];

    /* Number of reasonable P/V histograms obtained */
    int spe_cnt = 0;

    /* Peak to valley data -- allocate space for GAIN_CAL_HV_CNT pairs */
    pv_dat pv_data[GAIN_CAL_HV_CNT]; 

#ifdef DEBUG
    printf("Performing HV gain calibration...\r\n");
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
    
    /* Trigger ATWD B only */
    trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Get calibrated sampling frequency */
    float freq;
    short samp_dac = halReadDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);
    freq = getCalibFreq(0, *dom_calib, samp_dac);

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

    short hv_idx = 0;
    short hv;

    /* Loop over HV settings */
    for (hv_idx = 0; hv_idx < GAIN_CAL_HV_CNT; hv_idx++) {
        
        /* Set high voltage and give it time to stabilize */
        hv = (hv_idx * GAIN_CAL_HV_INC) + GAIN_CAL_HV_LOW;      

#ifdef DEBUG
        printf(" Setting HV to %d V\r\n", hv);
#endif

        halWriteActiveBaseDAC(hv * 2);
        halUSleep(5000000);
                
        /* Warm up the ATWD */
        prescanATWD(trigger_mask);
        
        for (trig=0; trig<(int)GAIN_CAL_TRIG_CNT; trig++) {
            
            /* Discriminator trigger the ATWD */
            hal_FPGA_TEST_trigger_disc(trigger_mask);
            
            /* Wait for done */
            while (!hal_FPGA_TEST_readout_done(trigger_mask));

            /* Read out one waveform from channels 0 and 1 */
            hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                  channels[0], channels[1], NULL, NULL, 
                                  cnt, NULL, 0, trigger_mask);            

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
            peak_v = (float)channels[ch][0] * dom_calib->atwd1_gain_calib[ch][0].slope
                + dom_calib->atwd1_gain_calib[ch][0].y_intercept;

            for (bin=0; bin<cnt; bin++) {

                /* Use calibration to convert to V */
                /* Don't need to subtract out bias or correct for amplification to find */
                /* peak location -- but without correction, it is really a minimum */
                bin_v = (float)channels[ch][bin] * dom_calib->atwd1_gain_calib[ch][bin].slope
                    + dom_calib->atwd1_gain_calib[ch][bin].y_intercept;

                if (bin_v < peak_v) {
                    peak_idx = bin;
                    peak_v = bin_v;
                }
            }

            /* Now integrate around the peak to get the charge */
            /* FIX ME: increase sampling speed? */
            /* FIX ME: use time window instead? */
            vsum = 0;
            for (bin = peak_idx-4; ((bin <= peak_idx+8) && (bin < cnt)); bin++)
                vsum += getCalibV(channels[ch][bin], *dom_calib, 1, ch, bin, bias_v);

            /* True charge, in pC = 1/R_ohm * sum(V) * 1e12 / (freq_mhz * 1e6) */
            /* FE sees a 50 Ohm load */
            charges[trig] = 0.02 * 1e6 * vsum / freq;

            /* TEMP TEMP TEMP */
            /* Print sample */
            /* if (trig == 0) {
                printf("Sample waveform\r\n");
                for (bin=0; bin<cnt; bin++) {
                    bin_v = (float)channels[ch][bin] * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept;
                    printf("%d %.6g\r\n", channels[ch][bin], bin_v);
                }
                printf("Peak: %.6g at %d\r\n", peak_v, peak_idx);
                printf("Charge: %.6g\r\n", charges[0]);                
                } */

        } /* End trigger loop */

        /* Create histogram of charge values */
        /* Heuristic maximum for histogram */
        int hist_max = ceil(pow(10.0, 6.37*log10(hv*2)-21.0));
        int hbin;

        /* Initialize histogram */
        for(hbin=0; hbin < GAIN_CAL_BINS; hbin++) {
	    hist_x[hv_idx][hbin] = (float)hbin * hist_max / GAIN_CAL_BINS;
	    hist_y[hv_idx][hbin] = 0.0;
	}

        /* Fill histogram -- histogram minimum is 0.0 */
        for(trig=0; trig < GAIN_CAL_TRIG_CNT; trig++) {

            hbin = charges[trig] * GAIN_CAL_BINS / hist_max;
            
            /* Do NOT use an overflow bin; will screw up fit */
            if (hbin < GAIN_CAL_BINS)
                hist_y[hv_idx][hbin] += 1;
        }

#ifdef DEBUG
        printf("Histogram:\r\n");
        for(hbin=0; hbin < GAIN_CAL_BINS; hbin++) 
            printf("%d %g %g\r\n", hbin, hist_x[hv_idx][hbin], hist_y[hv_idx][hbin]);
#endif

	/* Fit histograms */
	int fiterr;
	fiterr = spe_fit(hist_x[hv_idx], hist_y[hv_idx], GAIN_CAL_BINS, fit_params[hv_idx], GAIN_CAL_TRIG_CNT );
    
    /* If no error in fit, record gain and P/V */
    if (!fiterr) {

        float valley_x, valley_y, pv_ratio;
        /* Find valley */
        if (spe_find_valley(fit_params[hv_idx], &valley_x, &valley_y) == 0) {

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
                pv_data[spe_cnt].pv = pv_ratio;
                pv_data[spe_cnt].voltage = hv;
                
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
#endif
        }
    }
    else {
#ifdef DEBUG
        printf("Bad SPE fit: gain point at %d V not used\r\n", hv);
#endif
    }
    
    } /* End HV loop */

    /* Add P/V data to calib struct */
    dom_calib->num_pv = spe_cnt;
    dom_calib->pv_data = pv_data;

    if (spe_cnt >= 2) {
        /* Fit log(hv) vs. log(gain) */
        linearFitFloat(log_hv, log_gain, spe_cnt, &(dom_calib->hv_gain_calib)); 
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
