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
    float charges[3][AMP_CAL_TRIG_CNT];

    /* Charge arrays for higher gain channels, for later comparison, in Volts */
    float hcharges[3][AMP_CAL_TRIG_CNT];

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
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, AMP_CAL_SAMPLING_DAC);

    bias_v = biasDAC2V(AMP_CAL_PEDESTAL_DAC);

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
                                                                                                                                               
#ifdef DEBUG
    printf(" Setting HV to %d V\r\n", hv);
#endif
                                                                                                                                               
    halWriteActiveBaseDAC(hv * 2);
    halUSleep(5000000);

    /* Trigger one ATWD only */
    trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Loop over channels and pulser settings for each channel */
    for (ch = 1; ch < 3; ch++) {

        /* Check rate -- do we need to turn on LED? */
        float rate = 0;

        /* LED amplitude */
        short led_amplitude = LED_OFF;

        rate = measure_rate(atwd, ch-1);

        while (rate < MIN_PULSE_RATE) {

            /* OK -- not enough signal */
            /* Turn on MB LED if off -- otherwise inc amplitude until */
            /* we can see it */
            if (led_amplitude == LED_OFF) {
                 
                hal_FPGA_TEST_enable_LED();
                halEnableLEDPS();
                led_amplitude = LED_MAX_AMPLITUDE;
            } else {
                led_amplitude -= LED_AMPLITUDE_DEC;
            }

            /* Can't do calibration if rate is too low */
            if (led_amplitude < 0) {
                /* FIX ME -- flag failure using ch2 gain error */
                dom_calib->amplifier_calib[2].error = -1.0;

                /* Put the DACs back to original state */
                halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);
                halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
                halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS :
                                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);
                if (led_amplitude != LED_OFF) {
                    halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, old_led_value);
                    halDisableLEDPS();
                    hal_FPGA_TEST_disable_LED();
                }
                
                return ERR_LOW_RATE;
            }

            /* Apply new setting and wait */
            halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, led_amplitude);
            halUSleep(DAC_SET_WAIT);

            /* Measure rate */ 
            rate = measure_rate(atwd, ch-1);

            /* FIX ME DEBUG */
            printf("Rate: %f LED amplitude: %d\n", rate, led_amplitude); 

        }

        /* OK -- we have illumination.  Let's re-check the baseline because */
        /* just about everything affects it -- possibly even including the  */
        /* position of Jupiter and recent solar activity..... */

        float baseline[2][3];
        getBaseline(dom_calib, BASELINE_CAL_MAX_VAR, baseline);

        /* FIX ME DEBUG */
        int iter = 0;

 
        long long clk = hal_FPGA_TEST_get_local_clock();
        short cidx = 1;

        for (trig=0; trig<(int)AMP_CAL_TRIG_CNT; trig++) {

            iter++;

            /* Warm up the ATWD */
            prescanATWD(trigger_mask);
            
            /* Discriminator trigger the ATWD */
            hal_FPGA_TEST_trigger_disc(trigger_mask);
            
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

            /* FIX ME DEBUG */
            if (trig%10 == 0) {
                long long dt = hal_FPGA_TEST_get_local_clock() - clk;
                int time = dt / 40000;
                printf("Got trigger %d loop iteration %d time %dms\n", trig, iter, time);
            }

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
                if (bin == AMP_CAL_START_BIN) {
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
                for (i = 0; i < 2; i++) peak_vdat[i][bin] = current_v[i];
            }

            /* Integrate around peak bin */
            /* Set limits */
            int mins[2];
            int maxes[2];
            for (i = 0; i < 2; i++) {
                mins[i] = peak_bin[i] - CHARGE_FOW_BINS < AMP_CAL_START_BIN ? AMP_CAL_START_BIN : peak_bin[i] - CHARGE_FOW_BINS;
                maxes[i] = peak_bin[i] + CHARGE_REV_BINS > cnt-1 ? cnt-1 : peak_bin[i] + CHARGE_REV_BINS;
            }   

            float charge_data[2];
            for (i = 0; i < 2; i++) charge_data[i] = 0;

            /* Integrate charge */
            for (i = 0; i < 2; i++) {
                for (bin = mins[i]; bin <= maxes[i]; bin++) charge_data[i] += peak_vdat[i][bin];
            }

            charges[ch][trig] = charge_data[0];
            hcharges[ch-1][trig] = charge_data[1];

        }

        /* Turn off LED and LED PS between channel calibration */
        halDisableLEDPS();
        hal_FPGA_TEST_disable_LED();
        halUSleep(DAC_SET_WAIT);
    }

    float mean, var;
    for (ch = 1; ch < 3; ch++) {
        /* Divide by charges in higher gain ch to get amp ratio */
        /* Multiply by known amplification factor to find gain */
        for (bin = 0; bin < AMP_CAL_TRIG_CNT; bin++) {

            charges[ch][bin] /= hcharges[ch-1][bin];
            charges[ch][bin] *= dom_calib->amplifier_calib[ch-1].value;
        }

        /* Find mean and var */
        meanVarFloat(charges[ch], AMP_CAL_TRIG_CNT, &mean, &var);

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
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
    halWriteDAC((atwd == 0) ? DOM_HAL_DAC_ATWD0_TRIGGER_BIAS : 
                DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC);
    halWriteDAC(DOM_HAL_DAC_LED_BRIGHTNESS, old_led_value);

    /* Won't turn off the HV for now...*/

    /* FIX ME: return real error code */
    return 0;

}

float measure_rate(int atwd, int ch) {

    /* LED rate is >1KHz, so no prescan needed in this loop */

    /* ATWD readout */
    short channels[2][128];

    /* Trigger one ATWD only */
    int trigger_mask = (atwd == 0) ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;

    /* Readout length */
    int cnt = 128;

    /* Number of pulses above threshold */
    int p_cnt = 0;

    /* store start clk for rate measurement */
    long long clk_st = hal_FPGA_TEST_get_local_clock();

    long long clk;
    for (clk = hal_FPGA_TEST_get_local_clock();
            clk - clk_st < TEST_TRIG_TIME * FPGA_HAL_TICKS_PER_SEC;
            clk = hal_FPGA_TEST_get_local_clock()) {

        /* LED trigger the ATWD */
        hal_FPGA_TEST_trigger_disc(trigger_mask);


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

        /* Is pulse of acceptable amplitude */
        if (max > HV_AMP_CAL_MIN_PULSE && max < HV_AMP_CAL_MAX_PULSE) p_cnt++;
    }

    return (float)p_cnt / TEST_TRIG_TIME;
}
