
/*
 * Write the DOMCal XML file to STDOUT.  Captured by surface client.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "domcal.h"
#include "hv_gain_cal.h"
#include "write_xml.h"

/*--------------------------------------------------------------------*/

void write_xml(calib_data *dom_calib) {
    
    int i, atwd, ch, bin;

    /* Opening tag and version information */
    char version[10];
    sprintf(version, "%d.%d.%d", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    printf("<domcal version=\"%s\">\r\n", version);

    /* Vitals */
    printf("  <date>%d-%d-%d</date>\r\n", dom_calib->day, dom_calib->month, dom_calib->year);
    printf("  <time>%02d:%02d:%02d</time>\r\n", dom_calib->hour, dom_calib->minute, dom_calib->second);
    printf("  <domid>%s</domid>\r\n", dom_calib->dom_id);    
    printf("  <temperature format=\"Kelvin\">%.1f</temperature>\r\n", dom_calib->temp);

    /* DAC and ADC values */
    for (i = 0; i < 16; i++ )
        printf("  <dac channel=\"%d\">%d</dac>\r\n", i, dom_calib->dac_values[i]);
    for (i = 0; i < 24; i++ )
        printf("  <adc channel=\"%d\">%d</adc>\r\n", i, dom_calib->adc_values[i]);

    printf("  <frontEndImpedance format=\"Ohms\">%.1f</frontEndImpedance>\r\n", dom_calib->fe_impedance);

    /* SPE discriminator calibration */
    printf("  <discriminator id=\"spe\">\r\n");
    write_linear_fit(dom_calib->spe_disc_calib);    
    printf("  </discriminator>\r\n");

    /* MPE discriminator calibration */
    printf("  <discriminator id=\"mpe\">\r\n");
    write_linear_fit(dom_calib->mpe_disc_calib);
    printf("  </discriminator>\r\n");

    /* ATWD bin-by-bin calibration */
    for ( atwd = 0; atwd < 2; atwd++) {
        for ( ch = 0; ch < 3; ch++ ) {
            for ( bin = 0; bin < 128; bin++ ) {
                printf("  <atwd id=\"%d\" channel=\"%d\" bin=\"%d\">\r\n", atwd, ch, bin);
                if (atwd == 0)
                    write_linear_fit(dom_calib->atwd0_gain_calib[ch][bin]);
                else
                    write_linear_fit(dom_calib->atwd1_gain_calib[ch][bin]);
                printf("  </atwd>\r\n");
            }
        }
    }

    /* FADC baseline calibration */
    printf("  <fadc_baseline>\r\n");
    write_linear_fit(dom_calib->fadc_baseline);
    printf("  </fadc_baseline>\r\n");

    /* FADC gain calibration */
    printf("  <fadc_gain>\r\n");
    printf("    <gain error=\"%g\">%g</gain>\r\n", dom_calib->fadc_gain.error, dom_calib->fadc_gain.value);
    printf("  </fadc_gain>\r\n");

    /* FADC time offset calibration */
    printf("  <fadc_delta_t>\r\n");
    printf("    <delta_t error=\"%g\">%g</delta_t>\r\n", dom_calib->fadc_delta_t.error, dom_calib->fadc_delta_t.value);
    printf("  </fadc_delta_t>\r\n");

    /* ATWD channel gain calibration */
    for ( ch = 0; ch < 3; ch++) {
        printf("  <amplifier channel=\"%d\">\r\n", ch);
        printf("    <gain error=\"%g\">%g</gain>\r\n", 
               dom_calib->amplifier_calib[ch].error,
               dom_calib->amplifier_calib[ch].value);
        printf("  </amplifier>\r\n");
    }

    /* ATWD sampling frequency calibration */
    for ( atwd = 0; atwd < 2; atwd++) {
        printf("  <atwdfreq atwd=\"%d\">\r\n", atwd);
        if (atwd == 0)
            write_quadratic_fit(dom_calib->atwd0_freq_calib);
        else
            write_quadratic_fit(dom_calib->atwd1_freq_calib);
        printf("  </atwdfreq>\r\n");
    }

    /* HV off baseline calibration */
    printf("  <baseline voltage=\"0\">\r\n");
    for ( atwd = 0; atwd < 2; atwd++) {
        for ( ch = 0; ch < 3; ch++) {
            if (atwd == 0) {
                printf("    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                       atwd, ch, dom_calib->atwd0_baseline[ch]);
            }
            else {
                printf("    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                       atwd, ch, dom_calib->atwd1_baseline[ch]);
            }
        }
    }
    printf("  </baseline>\r\n");

    /* Domapp FPGA baseline calibration */
    if (dom_calib->daq_baselines_valid) {
        printf("  <daq_baseline>\r\n");
        for ( atwd = 0; atwd < 2; atwd++) {
            for ( ch = 0; ch < 3; ch++){
                for (bin = 0; bin < 128; bin++) {
                    printf("   <waveform atwd=\"%d\" channel=\"%d\" bin=\"%d\">", atwd, ch, bin);
                    if (atwd == 0)
                        printf("%g</waveform>\r\n", dom_calib->atwd0_daq_baseline_wf[ch][bin]);
                    else
                        printf("%g</waveform>\r\n", dom_calib->atwd1_daq_baseline_wf[ch][bin]);
                }
            }
        }
        printf("  </daq_baseline>\r\n");
    }

    /* Transit time calibration */
    if (dom_calib->transit_calib_valid) {
        printf("  <pmtTransitTime num_pts=\"%d\">\r\n", dom_calib->transit_calib_points);
        write_linear_fit(dom_calib->transit_calib);
        printf("  </pmtTransitTime>\r\n");
    }

    /* HV / Gain calibration */
    if (dom_calib->hv_gain_valid) {            
        printf("  <hvGainCal>\r\n");
        write_linear_fit(dom_calib->hv_gain_calib);
        printf("  </hvGainCal>\r\n");
    }

    /* Baseline measurements at various HV settings */
    if (dom_calib->hv_baselines_valid) {
        for (i = 0; i < dom_calib->num_baselines; i++) {
            hv_baselines *base = &(dom_calib->baseline_data[i]);
            printf("  <baseline voltage=\"%d\">\r\n", base->voltage);
            for ( atwd = 0; atwd < 2; atwd++) {
                for ( ch = 0; ch < 3; ch++) {
                    if (atwd == 0) {
                        printf("    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                               atwd, ch, base->atwd0_hv_baseline[ch]);
                    }
                    else {
                        printf("    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                               atwd, ch, base->atwd1_hv_baseline[ch]);
                    }
                }
            }
            printf("  </baseline>\r\n");
        }
    }

    /* HV histograms at various voltages, including fit parameters */
    for (i = 0; i < dom_calib->num_histos; i++)
        write_histogram(dom_calib->histogram_data[i]);

    printf("</domcal>\r\n");

}

/*--------------------------------------------------------------------*/
/* 
 * Write a linear fit element to STDOUT
 */
void write_linear_fit(linear_fit fit) {
    printf("    <fit model=\"linear\">\r\n");
    printf("      <param name=\"slope\">%g</param>\r\n", fit.slope);
    printf("      <param name=\"intercept\">%g</param>\r\n", fit.y_intercept);
    printf("      <regression-coeff>%g</regression-coeff>\r\n", fit.r_squared);
    printf("    </fit>\r\n");
}

/*--------------------------------------------------------------------*/
/*
 * Write a quadratic fit element to STDOUT
 */
void write_quadratic_fit(quadratic_fit fit) {
    printf("    <fit model=\"quadratic\">\r\n");
    printf("      <param name=\"c0\">%g</param>\r\n", fit.c0);
    printf("      <param name=\"c1\">%g</param>\r\n", fit.c1);
    printf("      <param name=\"c2\">%g</param>\r\n", fit.c2);
    printf("      <regression-coeff>%g</regression-coeff>\r\n", fit.r_squared);
    printf("    </fit>\r\n");
}

/*--------------------------------------------------------------------*/
/*
 * Write histogram data, fit parameters, etc. at a given voltage
 */
void write_histogram(hv_histogram histo) {

    int i;

    /* Don't write anything if histogram is not filled */
    if (histo.is_filled) {
        printf("  <histo voltage=\"%d\" convergent=\"%d\" pv=\"%.2f\" noiseRate=\"%g\" isFilled=\"%d\">\r\n", 
               histo.voltage, histo.convergent, histo.pv, histo.noise_rate, histo.is_filled);    
        printf("    <param name=\"exponential amplitude\">%g</param>\r\n", histo.fit[0]);
        printf("    <param name=\"exponential width\">%g</param>\r\n", histo.fit[1]);
        printf("    <param name=\"gaussian amplitude\">%g</param>\r\n", histo.fit[2]);
        printf("    <param name=\"gaussian mean\">%g</param>\r\n", histo.fit[3]);
        printf("    <param name=\"gaussian width\">%g</param>\r\n", histo.fit[4]);
        printf("    <histogram bins=\"%d\">\r\n", GAIN_CAL_BINS);
        for (i = 0; i < GAIN_CAL_BINS; i++) {
            printf("      <bin num=\"%d\" charge=\"%.4f\" count=\"%d\"></bin>\r\n", 
                   i, histo.x_data[i], (int)histo.y_data[i]);
        }
        printf("    </histogram>\r\n");
        printf("  </histo>\r\n");
    }

    /* Add a little delay as to not overwhelm the surface */
    halUSleep(250000);
}
