/*
 * hv_gain_cal
 *
 * HV gain calibration routine -- integrates SPE waveforms
 * to create charge histogram, and then fits to find peak and valley.
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
    float hist_y[GAIN_CAL_HV_CNT][HIST_BIN_CNT];

    /* Actual x-values for each histgram */
    float hist_x[GAIN_CAL_HV_CNT][HIST_BIN_CNT];

    /* Fit parameters for each SPE spectrum */
    float fit_params[GAIN_CAL_HV_CNT][SPE_FIT_PARAMS];    


#ifdef DEBUG
    printf("Performing HV gain calibration...\r\n");
#endif

    /* Save DACs that we modify */
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);

    /* Set discriminator */
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, GAIN_CAL_DISC_DAC);

    /* Get bias DAC setting */
    bias_dac = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);

    /* Make sure pulser is off */
    hal_FPGA_TEST_disable_pulser();

    /* Select something static into channel 3 */
    halSelectAnalogMuxInput(DOM_HAL_MUX_FLASHER_LED_CURRENT);
    
    /* Trigger ATWD A only */
    trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0;

    /* Get calibrated sampling frequency */
    float freq;
    short samp_dac = halReadDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS);
    freq = getCalibFreq(0, *dom_calib, samp_dac);

    /* Give user a final warning */
#ifdef DEBUG
    printf(" *** WARNING: enabling HV in 5 seconds! ***\r\n");
#endif
    halUSleep(5000000);

    /* Turn on high voltage base */
#ifdef REV3HAL
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
        halUSleep(2500000);
                
        /* Warm up the ATWD */
        prescanATWD(trigger_mask);
        
        for (trig=0; trig<(int)GAIN_CAL_TRIG_CNT; trig++) {
            
            /* Discriminator trigger the ATWD */
            hal_FPGA_TEST_trigger_disc(trigger_mask);
            
            /* Wait for done */
            while (!hal_FPGA_TEST_readout_done(trigger_mask));

            /* Read out one waveform from channels 0 and 1 */
            hal_FPGA_TEST_readout(channels[0], channels[1], NULL, NULL, 
                                  NULL, NULL, NULL, NULL,
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

            /* Find the peak (maximum) */
            peak_idx = 0;
            peak_v =  getCalibV(channels[ch][0], *dom_calib, 0, ch, bin, bias_dac);

            for (bin=0; bin<cnt; bin++) {

                /* Use calibration to convert to V */
                bin_v = getCalibV(channels[ch][bin], *dom_calib, 0, ch, bin, bias_dac);

                if (bin_v > peak_v) {
                    peak_idx = bin;
                    peak_v = bin_v;
                }
            }

            /* Now integrate around the peak to get the charge */
            /* FIX ME: if peak_idx is close to max, is this correct */
            /* FIX ME: use time window instead? */
            vsum = 0;
            for (bin = peak_idx-4; ((bin <= peak_idx+8) && (bin < cnt)); bin++)
                vsum += getCalibV(channels[ch][bin], *dom_calib, 0, ch, bin, bias_dac);

            /* True charge, in pC = 1/R_ohm * sum(V) * 1e12 / (freq_mhz * 1e6) */
            /* FE sees a 50 Ohm load */
            charges[trig] = 0.02 * 1e6 * vsum / freq;

            /* TEMP */
            /* printf("Peak of previous wf: %d %g\r\n", peak_idx, peak_v);
               printf("Charge of previous wf: %g\r\n", charges[trig]); */

        } /* End trigger loop */

        /* Create histogram of charge values */
        /* Heuristic maximum for histogram */
        int hist_max = ceil(pow(10.0, 6.37*log10(hv*2)-21.0));
        int hbin;
        
        printf("Histogram max = %d\r\n", hist_max);

        /* Initialize histogram */
        for(hbin=0; hbin < HIST_BIN_CNT; hbin++) {
	    hist_x[hv_idx][hbin] = (float)hbin * hist_max / HIST_BIN_CNT;
	    hist_y[hv_idx][hbin] = 0.0;
	}

        /* Fill histogram -- histogram minimum is 0.0 */
        for(trig=0; trig < GAIN_CAL_TRIG_CNT; trig++) {

            hbin = charges[trig] * HIST_BIN_CNT / hist_max;
            
            /* Do NOT use an overflow bin; will screw up fit */
            if (hbin < HIST_BIN_CNT)
                hist_y[hv_idx][hbin] += 1;
        }

#ifdef DEBUG
        printf("Histogram: \r\n");
        for(hbin=0; hbin < HIST_BIN_CNT; hbin++) 
            printf("%d %g %g\r\n", hbin, hist_x[hv_idx][hbin], hist_y[hv_idx][hbin]);
#endif

	/* Fit histograms */
	int fiterr;
	fiterr = spe_fit(hist_x[hv_idx], hist_y[hv_idx], 
			 HIST_BIN_CNT, fit_params[hv_idx]);

    } /* End HV loop */   

    /* Turn off the high voltage */
#ifdef REV3HAL
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
