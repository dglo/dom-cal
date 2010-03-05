/*
 * hv_amp_cal
 *
 * Amplifier recalibration routine -- record waveform peaks in 
 * ATWD channels for muons and use ATWD calibration to find 
 * gain of each amplifier.  Uses only one ATWD.
 *
 */

#include <stdio.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "amp_cal.h"
#include "hv_amp_cal.h"
#include "calUtils.h"
#include "baseline_cal.h"

/*---------------------------------------------------------------------------*/
int hv_amp_cal(calib_data *dom_calib) {

    const int cnt = 128;
    int trigger_mask;
    int ch, bin, trig, i;
    float bias_v;
    float peak_vdat[2][128];

    int hv;
    
    /* Which atwd to use */
    short atwd = AMP_CAL_ATWD;

    /* Channel readout buffers for each channel and bin */
    /* This test only uses one ATWD */
    short channels[3][128];

    /* Charge arrays for each channel, in Volts */
    float charges[3][HV_AMP_CAL_TRIG_CNT];

    /* Charge arrays for higher gain channels, for later comparison, in Volts */
    float hcharges[3][HV_AMP_CAL_TRIG_CNT];

#ifdef DEBUG
    printf("Performing HV amplifier calibration (using ATWD%d)...\r\n", atwd);
#endif

    /* Save DACs that we modify */
    short origBiasDAC = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);
    /* Increase sampling speed to make sure we see peak */
    short origSampDAC = halReadDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                                   DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);
    short old_led_value = halReadDAC(DOM_HAL_DAC_LED_BRIGHTNESS);

    /* Set discriminator and bias level */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, AMP_CAL_PEDESTAL_DAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, HV_AMP_CAL_DISC_DAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, HV_AMP_CAL_SAMPLING_DAC);

    bias_v = biasDAC2V(AMP_CAL_PEDESTAL_DAC);

    /* Get the sampling speed */
    float freq = getCalibFreq(atwd, *dom_calib, HV_AMP_CAL_SAMPLING_DAC);
    
    /* Set the ATWD launch delay */
    hal_FPGA_TEST_set_atwd_LED_delay(HV_AMP_CAL_ATWD_LAUNCH_DELAY);

    /* Turn on high voltage base */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halEnablePMT_HV();
#else
    halPowerUpBase();
    halEnableBaseHV();
#endif

    /* Ensure HV base exists before performing calibration */
    if (!checkHVBase()) return 0;

    hv = HV_AMP_CAL_VOLTS;

    /* Check to make sure we're not exceeding max requested HV */
    hv = (hv > dom_calib->max_hv) ? dom_calib->max_hv : hv;

#ifdef DEBUG
    printf(" Setting HV to %d V\r\n", hv);
#endif
                                                                                                                                               
    halWriteActiveBaseDAC(hv * 2);
    halUSleep(5000000);

    /* Trigger one ATWD only */
    trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Build a rough histogram to find PMT saturation point */
    float av[LED_MAX_AMPLITUDE];
    int sat = -1;

    for (i = 0; i < LED_MAX_AMPLITUDE; i++) av[i] = 0.0;

    /* Turn on LED */
    hal_FPGA_TEST_enable_LED();
    halEnableLEDPS();

    /* Fill histogram */
    int led_amplitude;
    for (led_amplitude = LED_MAX_AMPLITUDE - 1; led_amplitude >= 0;
                                             led_amplitude -= HV_AMP_CAL_HISTOGRAM_DEC) {

        /* Set new amplitude */
        halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, led_amplitude);
        halUSleep(DAC_SET_WAIT);

        /* Calculate our average atwd response to LED signal */
        av[led_amplitude] = get_average_amplitude(atwd, 1, HV_AMP_CAL_HISTOGRAM_TRIG_TIME);
        printf("LED response: DAC: %d Amp: %f\n", led_amplitude, av[led_amplitude]);

        /* Check whether we need to continue */
        if (av[led_amplitude] > 800.0) {
            sat = led_amplitude;
            break;
        }
    }

    if (sat == -1) {
        /* OK -- we didn't find a saturation point -- maybe we have a weak LED */
        /* Find maximum */
        float max = 0.0;
        for (i = LED_MAX_AMPLITUDE - 1; i >= 0; i -= HV_AMP_CAL_HISTOGRAM_DEC) {
            if (av[i] > max) {
                max = av[i];
                sat = i;
            }
        }
    }

    /* Reset LED DAC, let pmt recover */
    halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, LED_MAX_AMPLITUDE);
    halUSleep(DAC_SET_WAIT);        

    /* Loop over channels and pulser settings for each channel */
    /* Start with brightest channel to reduce chance of interference with other DOMs' HV gain cal */
    for (ch = 2; ch >= 1; ch--) {

        /* Do binary search to place average LED pulse at prime level */
        int l_max = LED_MAX_AMPLITUDE;
        int l_min = sat;

        /* We stop when max and min agree */
        while (l_max - l_min > 0) {

            /* Calculate and set new amplitude */
            led_amplitude = (l_max + l_min) / 2;
            halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, led_amplitude);
            halUSleep(DAC_SET_WAIT);

            /* Calculate our average atwd response to LED signal */
            float lev = get_average_amplitude(atwd, ch-1, TEST_TRIG_TIME);

            /* FIX ME debug */
            printf("Search: DAC: %d Level: %f\n", led_amplitude, lev);

            /* Are we above prime or below? */
            if (lev < HV_AMP_CAL_BEST_PULSE) {
                if (l_max - l_min > 1) {
                    l_max = led_amplitude;
                } else {
                    l_max--;
                }
            } else {
                if (l_max - l_min > 1) {
                    l_min = led_amplitude;
                } else {
                    l_min++;
                }
            }
        }

        /* Set LED to calculated best setting */
        halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, l_max);
        halUSleep(DAC_SET_WAIT);

        /* OK -- we have illumination.  Let's re-check the baseline because */
        /* just about everything affects it -- possibly even including the  */
        /* position of Jupiter and recent solar activity..... */

        float baseline[2][3];
        getBaseline(dom_calib, BASELINE_CAL_MAX_VAR, baseline);

        /* FIX ME DEBUG */
        int iter = 0;

        /* Record start time */
        long long clk = hal_FPGA_TEST_get_local_clock();
        short cidx = 1;

        for (trig=0; trig<(int)HV_AMP_CAL_TRIG_CNT; trig++) {

            iter++;

            /* Warm up the ATWD */
            prescanATWD(trigger_mask);
            
            /* LED trigger the ATWD */
            hal_FPGA_TEST_trigger_LED(trigger_mask);
            
            /* Wait for done */
            while (!hal_FPGA_TEST_readout_done(trigger_mask)) {

                /* Check for timeout every 2^16 wait cycles */
                if (cidx++ == 0) {
                    
                    long long dt = hal_FPGA_TEST_get_local_clock()-clk;
                    int ms = (int)(dt / 40000);
                    if (ms > MAX_AMP_CAL_ITER_MS) {

                        /* We've timed out */
                        /* FIX ME -- flag failure using ch2 gain error */
                        dom_calib->amplifier_calib[2].error = -1.0;

                        /* Put the DACs back to original state */
                        halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);
                        halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
                        halWriteDAC((atwd == 0) ? 
                                DOM_HAL_DAC_ATWD0_TRIGGER_BIAS :
                                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);
                        halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, old_led_value);
                        halDisableLEDPS();
                        hal_FPGA_TEST_disable_LED();

                        return ERR_TIMEOUT;
                    }
                }
            }
                

            /* Read out one waveform for all channels except 3 */        
            if (atwd == 0) {
                hal_FPGA_TEST_readout(channels[0], channels[1], channels[2], NULL, 
                                      NULL, NULL, NULL, NULL,
                                      cnt, NULL, 0, trigger_mask);
            }
            else {
                hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                      channels[0], channels[1], channels[2], NULL,
                                      cnt, NULL, 0, trigger_mask);
            }

            /* Look at raw ATWD data of higher gain channel, whose amplifier should be
             * calibrated already, to check range
             */

            short max = 0;
            for (bin = HV_AMP_CAL_START_BIN; bin < cnt; bin++) max = channels[ch-1][bin] > max ? 
                                                             channels[ch-1][bin] : max;
            if (max < HV_AMP_CAL_MIN_PULSE || max > HV_AMP_CAL_MAX_PULSE) {
                trig--;
                continue;
            }

            /* OK -- we have a reasonable muon pulse */

            float current_v[2];
            float peak_v[2];
            int peak_bin[2];

            /* Find and record charge.  Make sure it is in range */ 
            for (bin=HV_AMP_CAL_START_BIN; bin<cnt; bin++) {


                /* Using ATWD calibration data, convert to actual V */
                if (atwd == 0) {
                    current_v[0] = (float)(channels[ch][bin]) * dom_calib->atwd0_gain_calib[ch][bin].slope
                        + dom_calib->atwd0_gain_calib[ch][bin].y_intercept
                        - baseline[atwd][ch];
                    current_v[1] = (float)(channels[ch-1][bin]) * dom_calib->atwd0_gain_calib[ch-1][bin].slope
                        + dom_calib->atwd0_gain_calib[ch-1][bin].y_intercept
                        - baseline[atwd][ch-1];
                }
                else {
                    current_v[0] = (float)(channels[ch][bin]) * dom_calib->atwd1_gain_calib[ch][bin].slope
                        + dom_calib->atwd1_gain_calib[ch][bin].y_intercept
                        - baseline[atwd][ch];
                    current_v[1] = (float)(channels[ch-1][bin]) * dom_calib->atwd1_gain_calib[ch-1][bin].slope
                        + dom_calib->atwd1_gain_calib[ch-1][bin].y_intercept
                        - baseline[atwd][ch-1];
                }

                /* Also subtract out bias voltage */
                for (i = 0; i < 2; i++) current_v[i] -= bias_v;

                /* Note "peak" is actually a minimum */
                if (bin == HV_AMP_CAL_START_BIN) {
                    for (i = 0; i < 2; i++) {
                        peak_v[i] = current_v[i];
                        peak_bin[i] = bin;
                    } 
                } else {
                    for (i = 0; i < 2; i++) {
                        if (current_v[i] < peak_v[i]) {
                            peak_bin[i] = bin;
                            peak_v[i] = current_v[i];
                        }
                    }
                }
                        
                /* Save voltage data for integration */
                for (i = 0; i < 2; i++)
                    peak_vdat[i][bin] = current_v[i];
            }

            /* Integrate around peak bin */
            /* Set limits */
            int mins[2];
            int maxes[2];
            int charge_fow_bins = (int)(CHARGE_FOW_NS * freq / 1000.0 + 1);
            int charge_rev_bins = (int)(CHARGE_REV_NS * freq / 1000.0 + 1);
            for (i = 0; i < 2; i++) {
                mins[i] = peak_bin[i] - charge_fow_bins < HV_AMP_CAL_START_BIN ? 
                                                    HV_AMP_CAL_START_BIN : peak_bin[i] - charge_fow_bins;
                maxes[i] = peak_bin[i] + charge_rev_bins > cnt-1 ? cnt-1 : peak_bin[i] + charge_rev_bins;
            }   

            float charge_data[2];
            for (i = 0; i < 2; i++) charge_data[i] = 0;

            /* Integrate charge */
            for (i = 0; i < 2; i++) {
                for (bin = mins[i]; bin <= maxes[i]; bin++) charge_data[i] += peak_vdat[i][bin];
            }

            charges[ch][trig] = charge_data[0];
            hcharges[ch-1][trig] = charge_data[1];
                        
        } /* End trigger loop */

        /* Turn down LED between channel calibration */
        halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, LED_MAX_AMPLITUDE);
        halUSleep(DAC_SET_WAIT);
    } /* End channel loop */

    float mean, var;
    for (ch = 1; ch < 3; ch++) {
        /* Divide by charges in higher gain ch to get amp ratio */
        /* Multiply by known amplification factor to find gain */
        for (bin = 0; bin < HV_AMP_CAL_TRIG_CNT; bin++) {

            charges[ch][bin] /= hcharges[ch-1][bin];
            charges[ch][bin] *= dom_calib->amplifier_calib[ch-1].value;
        }

        /* Find mean and var */
        meanVarFloat(charges[ch], HV_AMP_CAL_TRIG_CNT, &mean, &var);

        /* FIX ME */
#ifdef DEBUG
        printf("Old gain for ch %d: %g\r\n", ch, dom_calib->amplifier_calib[ch].value);
        printf("Old error for ch %d: %g\r\n", ch, dom_calib->amplifier_calib[ch].error);
#endif

        dom_calib->amplifier_calib[ch].value = mean;
        dom_calib->amplifier_calib[ch].error = var;

        /* FIX ME */
#ifdef DEBUG
        printf("Gain for ch %d: %g\r\n", ch, (mean));
        printf("Error for ch %d: %g\r\n", ch, (var));
#endif
    }

    /* Put the DACs back to original state */
    halDisableLEDPS();
    hal_FPGA_TEST_disable_LED();
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);
    halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, old_led_value);

    /* Won't disable HV ATTM */

    /* FIX ME: return real error code */
    return 0;

}

float get_average_amplitude(int atwd, int ch, int trig_time_sec) {

    /* NOTES:
     * construct peak histograms instead?  Possibly better performance.
     */

    /* LED rate is 610Hz, so no prescan needed in this loop */

    /* ATWD readout */
    short channels[2][128];

    /* Trigger one ATWD only */
    int trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Readout length */
    int cnt = 128;

    /* Number of pulses above threshold */
    int ct = 0;

    /* Sum of maximum amplitudes */
    int psum = 0;

    /* store start clk for rate measurement */
    long long clk_st = hal_FPGA_TEST_get_local_clock();

    long long clk;
    for (clk = hal_FPGA_TEST_get_local_clock();
            clk - clk_st < trig_time_sec * FPGA_HAL_TICKS_PER_SEC;
            clk = hal_FPGA_TEST_get_local_clock()) {

        /* LED trigger the ATWD */
        hal_FPGA_TEST_trigger_LED(trigger_mask);


        /* Wait for done */
        while (!hal_FPGA_TEST_readout_done(trigger_mask));


        /* Read out one waveform for all channels except 2, 3 */
        if (atwd == 0) {
            hal_FPGA_TEST_readout(channels[0], channels[1], NULL , NULL,
                                      NULL, NULL, NULL, NULL,
                                      cnt, NULL, 0, trigger_mask);
        }
        else {
            hal_FPGA_TEST_readout(NULL, NULL, NULL, NULL,
                                  channels[0], channels[1], NULL, NULL,
                                  cnt, NULL, 0, trigger_mask);
        }

        /* Find max from raw ATWD data */
        short max = 0;
        int bin;
        for (bin = HV_AMP_CAL_START_BIN; bin < cnt; bin++) {
            if (channels[ch][bin] > max) max = channels[ch][bin];
        }

        psum += max;
        ct++;
    }

    /* Return average peak amplitude */
    return (float)psum / ct;
}
