
/*
 * Write the DOMCal XML file to STDOUT.  Captured by surface client.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"
#include "zlib.h"

#include "domcal.h"
#include "hv_gain_cal.h"
#include "write_xml.h"

/*--------------------------------------------------------------------*/

int write_xml(calib_data *dom_calib, int throttle, int compress) {
    
    int i, atwd, ch, bin;

    /* Buffer for output */
    char linebuf[1000];

    /* Initialize zlib stream if compressing */
    z_stream zs;
    if (compress) {
        zs.next_in = Z_NULL;
        zs.zalloc = Z_NULL; 
        zs.zfree = Z_NULL; 
        zs.opaque = Z_NULL;
        deflateInit(&zs, 5);
    }

    /* Opening tag and version information */
    char version[10];
    sprintf(version, "%d.%d.%d", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    sprintf(linebuf, "<domcal version=\"%s\">\r\n", version);
    zprintstr(linebuf, compress, &zs, 0);

    /* Vitals */
    sprintf(linebuf, "  <date>%d-%d-%d</date>\r\n", dom_calib->day, dom_calib->month, dom_calib->year);
    zprintstr(linebuf, compress, &zs, 0);
    sprintf(linebuf, "  <time>%02d:%02d:%02d</time>\r\n", dom_calib->hour, dom_calib->minute, dom_calib->second);
    zprintstr(linebuf, compress, &zs, 0);
    sprintf(linebuf, "  <domid>%s</domid>\r\n", dom_calib->dom_id);    
    zprintstr(linebuf, compress, &zs, 0);
    sprintf(linebuf, "  <temperature format=\"Kelvin\">%.1f</temperature>\r\n", dom_calib->temp);
    zprintstr(linebuf, compress, &zs, 0);

    /* DAC and ADC values */
    for (i = 0; i < 16; i++ ) {
        sprintf(linebuf, "  <dac channel=\"%d\">%d</dac>\r\n", i, dom_calib->dac_values[i]);
        zprintstr(linebuf, compress, &zs, 0);
        halUSleep(10000 * throttle);
    }
    for (i = 0; i < 24; i++ ) {
        sprintf(linebuf, "  <adc channel=\"%d\">%d</adc>\r\n", i, dom_calib->adc_values[i]);
        zprintstr(linebuf, compress, &zs, 0);
        halUSleep(10000 * throttle);
    }

    sprintf(linebuf, "  <frontEndImpedance format=\"Ohms\">%.1f</frontEndImpedance>\r\n", dom_calib->fe_impedance);
    zprintstr(linebuf, compress, &zs, 0);

    /* SPE discriminator calibration */
    sprintf(linebuf, "  <discriminator id=\"spe\">\r\n");
    zprintstr(linebuf, compress, &zs, 0);
    write_linear_fit(dom_calib->spe_disc_calib, compress, &zs);    
    sprintf(linebuf, "  </discriminator>\r\n");
    zprintstr(linebuf, compress, &zs, 0);

    /* MPE discriminator calibration */
    sprintf(linebuf, "  <discriminator id=\"mpe\">\r\n");
    zprintstr(linebuf, compress, &zs, 0);
    write_linear_fit(dom_calib->mpe_disc_calib, compress, &zs);
    sprintf(linebuf, "  </discriminator>\r\n");
    zprintstr(linebuf, compress, &zs, 0);

    /* ATWD bin-by-bin calibration */
    for ( atwd = 0; atwd < 2; atwd++) {
        for ( ch = 0; ch < 3; ch++ ) {
            for ( bin = 0; bin < 128; bin++ ) {
                sprintf(linebuf, "  <atwd id=\"%d\" channel=\"%d\" bin=\"%d\">\r\n", atwd, ch, bin);
                zprintstr(linebuf, compress, &zs, 0);
                if (atwd == 0)
                    write_linear_fit(dom_calib->atwd0_gain_calib[ch][bin], compress, &zs);
                else
                    write_linear_fit(dom_calib->atwd1_gain_calib[ch][bin], compress, &zs);
                sprintf(linebuf, "  </atwd>\r\n");
                zprintstr(linebuf, compress, &zs, 0);
                halUSleep(10000 * throttle);
            }
        }
    }

    /* FADC baseline calibration */
    sprintf(linebuf, "  <fadc_baseline>\r\n");
    zprintstr(linebuf, compress, &zs, 0);
    write_linear_fit(dom_calib->fadc_baseline, compress, &zs);
    sprintf(linebuf, "  </fadc_baseline>\r\n");
    zprintstr(linebuf, compress, &zs, 0);

    /* FADC gain calibration */
    sprintf(linebuf, "  <fadc_gain>\r\n");
    zprintstr(linebuf, compress, &zs, 0);
    sprintf(linebuf, "    <gain error=\"%g\">%g</gain>\r\n", dom_calib->fadc_gain.error, dom_calib->fadc_gain.value);
    zprintstr(linebuf, compress, &zs, 0);
    sprintf(linebuf, "  </fadc_gain>\r\n");
    zprintstr(linebuf, compress, &zs, 0);

    /* FADC time offset calibration */
    sprintf(linebuf, "  <fadc_delta_t>\r\n");
    zprintstr(linebuf, compress, &zs, 0);
    sprintf(linebuf, "    <delta_t error=\"%g\">%g</delta_t>\r\n", 
            dom_calib->fadc_delta_t.error, dom_calib->fadc_delta_t.value);
    zprintstr(linebuf, compress, &zs, 0);
    sprintf(linebuf, "  </fadc_delta_t>\r\n");
    zprintstr(linebuf, compress, &zs, 0);

    /* ATWD time offset calibration */
    for (atwd = 0; atwd < 2; atwd++) {
        sprintf(linebuf, "  <atwd_delta_t id=\"%d\">\r\n", atwd);
        zprintstr(linebuf, compress, &zs, 0);
        sprintf(linebuf, "    <delta_t error=\"%g\">%g</delta_t>\r\n", dom_calib->atwd_delta_t[atwd].error, 
                dom_calib->atwd_delta_t[atwd].value);
        zprintstr(linebuf, compress, &zs, 0);
        sprintf(linebuf, "  </atwd_delta_t>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
    }

    /* ATWD channel gain calibration */
    for ( ch = 0; ch < 3; ch++) {
        sprintf(linebuf, "  <amplifier channel=\"%d\">\r\n", ch);
        zprintstr(linebuf, compress, &zs, 0);
        sprintf(linebuf, "    <gain error=\"%g\">%g</gain>\r\n", 
               dom_calib->amplifier_calib[ch].error,
               dom_calib->amplifier_calib[ch].value);
        zprintstr(linebuf, compress, &zs, 0);
        sprintf(linebuf, "  </amplifier>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
        halUSleep(100000 * throttle);
    }

    /* ATWD sampling frequency calibration */
    for ( atwd = 0; atwd < 2; atwd++) {
        sprintf(linebuf, "  <atwdfreq atwd=\"%d\">\r\n", atwd);
        zprintstr(linebuf, compress, &zs, 0);
        if (atwd == 0)
            write_quadratic_fit(dom_calib->atwd0_freq_calib, compress, &zs);
        else
            write_quadratic_fit(dom_calib->atwd1_freq_calib, compress, &zs);
        sprintf(linebuf, "  </atwdfreq>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
    }

    /* HV off baseline calibration */
    sprintf(linebuf, "  <baseline voltage=\"0\">\r\n");
    zprintstr(linebuf, compress, &zs, 0);
    for ( atwd = 0; atwd < 2; atwd++) {
        for ( ch = 0; ch < 3; ch++) {
            if (atwd == 0) {
                sprintf(linebuf, "    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                       atwd, ch, dom_calib->atwd0_baseline[ch]);
                zprintstr(linebuf, compress, &zs, 0);
            }
            else {
                sprintf(linebuf, "    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                       atwd, ch, dom_calib->atwd1_baseline[ch]);
                zprintstr(linebuf, compress, &zs, 0);
            }
            halUSleep(100000 * throttle);
        }
    }
    sprintf(linebuf, "  </baseline>\r\n");
    zprintstr(linebuf, compress, &zs, 0);

    /* Domapp FPGA baseline calibration */
    if (dom_calib->daq_baselines_valid) {
        sprintf(linebuf, "  <daq_baseline>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
        for ( atwd = 0; atwd < 2; atwd++) {
            for ( ch = 0; ch < 3; ch++){
                for (bin = 0; bin < 128; bin++) {
                    sprintf(linebuf, "   <waveform atwd=\"%d\" channel=\"%d\" bin=\"%d\">", atwd, ch, bin);
                    zprintstr(linebuf, compress, &zs, 0);
                    if (atwd == 0) {
                        sprintf(linebuf, "%g</waveform>\r\n", dom_calib->atwd0_daq_baseline_wf[ch][bin]);
                        zprintstr(linebuf, compress, &zs, 0);
                    }
                    else {
                        sprintf(linebuf, "%g</waveform>\r\n", dom_calib->atwd1_daq_baseline_wf[ch][bin]);
                        zprintstr(linebuf, compress, &zs, 0);
                    }
                    halUSleep(10000 * throttle);
                }
            }
        }
        sprintf(linebuf, "  </daq_baseline>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
    }

    /* Transit time calibration */
    if (dom_calib->transit_calib_valid) {
        sprintf(linebuf, "  <pmtTransitTime num_pts=\"%d\">\r\n", dom_calib->transit_calib_points);
        zprintstr(linebuf, compress, &zs, 0);
        write_linear_fit(dom_calib->transit_calib, compress, &zs);
        sprintf(linebuf, "  </pmtTransitTime>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
    }

    /* HV / Gain calibration */
    if (dom_calib->hv_gain_valid) {            
        sprintf(linebuf, "  <hvGainCal>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
        write_linear_fit(dom_calib->hv_gain_calib, compress, &zs);
        sprintf(linebuf, "  </hvGainCal>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
    }

    /* PMT discriminator calibration */
    if (dom_calib->pmt_disc_calib_valid) {            
        sprintf(linebuf, "  <pmtDiscCal num_pts=\"%d\">\r\n",
                                  dom_calib->pmt_disc_calib_num_pts);
        zprintstr(linebuf, compress, &zs, 0);
        write_linear_fit(dom_calib->pmt_disc_calib, compress, &zs);
        sprintf(linebuf, "  </pmtDiscCal>\r\n");
        zprintstr(linebuf, compress, &zs, 0);
    }
    halUSleep(1000000 * throttle);

    /* Baseline measurements at various HV settings */
    if (dom_calib->hv_baselines_valid) {
        for (i = 0; i < dom_calib->num_baselines; i++) {
            hv_baselines *base = &(dom_calib->baseline_data[i]);
            sprintf(linebuf, "  <baseline voltage=\"%d\">\r\n", base->voltage);
            zprintstr(linebuf, compress, &zs, 0);
            for ( atwd = 0; atwd < 2; atwd++) {
                for ( ch = 0; ch < 3; ch++) {
                    if (atwd == 0) {
                        sprintf(linebuf, "    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                               atwd, ch, base->atwd0_hv_baseline[ch]);
                        zprintstr(linebuf, compress, &zs, 0);
                    }
                    else {
                        sprintf(linebuf, "    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                               atwd, ch, base->atwd1_hv_baseline[ch]);
                        zprintstr(linebuf, compress, &zs, 0);
                    }
                }                
                halUSleep(10000 * throttle);           
            }
            sprintf(linebuf, "  </baseline>\r\n");
            zprintstr(linebuf, compress, &zs, 0);
        }
    }

    /* HV histograms at various voltages, including fit parameters */
    for (i = 0; i < dom_calib->num_histos; i++) {
        write_histogram(dom_calib->histogram_data[i], compress, &zs);
        halUSleep(500000 * throttle);
    }
    sprintf(linebuf, "</domcal>\r\n");
    zprintstr(linebuf, compress, &zs, 1);

    if (compress)
        deflateEnd(&zs);

    halUSleep(1000000 * throttle);

    return 0;
}

/*--------------------------------------------------------------------*/
/* 
 * Write a linear fit element to STDOUT
 */
void write_linear_fit(linear_fit fit, int compress, z_stream *zs) {

    char buf[2000];

    sprintf(buf, "    <fit model=\"linear\">\r\n");
    sprintf(buf, "%s      <param name=\"slope\">%g</param>\r\n", buf, fit.slope);
    sprintf(buf, "%s      <param name=\"intercept\">%g</param>\r\n", buf, fit.y_intercept);
    sprintf(buf, "%s      <regression-coeff>%g</regression-coeff>\r\n", buf, fit.r_squared);
    sprintf(buf, "%s    </fit>\r\n", buf);
    
    zprintstr(buf, compress, zs, 0);
}

/*--------------------------------------------------------------------*/
/*
 * Write a quadratic fit element to STDOUT
 */
void write_quadratic_fit(quadratic_fit fit, int compress, z_stream *zs) {
    char buf[2000];

    sprintf(buf, "    <fit model=\"quadratic\">\r\n");
    sprintf(buf, "%s      <param name=\"c0\">%g</param>\r\n", buf, fit.c0);
    sprintf(buf, "%s      <param name=\"c1\">%g</param>\r\n", buf, fit.c1);
    sprintf(buf, "%s      <param name=\"c2\">%g</param>\r\n", buf, fit.c2);
    sprintf(buf, "%s      <regression-coeff>%g</regression-coeff>\r\n", buf, fit.r_squared);
    sprintf(buf, "%s    </fit>\r\n", buf);

    zprintstr(buf, compress, zs, 0);
}

/*--------------------------------------------------------------------*/
/*
 * Write histogram data, fit parameters, etc. at a given voltage
 */
void write_histogram(hv_histogram histo, int compress, z_stream *zs) {

    int i;
    char linebuf[1000];

    /* Don't write anything if histogram is not filled */
    if (histo.is_filled) {
        sprintf(linebuf, "  <histo voltage=\"%d\" convergent=\"%d\" pv=\"%.2f\" noiseRate=\"%g\" isFilled=\"%d\">\r\n", 
               histo.voltage, histo.convergent, histo.pv, histo.noise_rate, histo.is_filled);    
        zprintstr(linebuf, compress, zs, 0);
        sprintf(linebuf, "    <param name=\"exponential amplitude\">%g</param>\r\n", histo.fit[0]);
        zprintstr(linebuf, compress, zs, 0);
        sprintf(linebuf, "    <param name=\"exponential width\">%g</param>\r\n", histo.fit[1]);
        zprintstr(linebuf, compress, zs, 0);
        sprintf(linebuf, "    <param name=\"gaussian amplitude\">%g</param>\r\n", histo.fit[2]);
        zprintstr(linebuf, compress, zs, 0);
        sprintf(linebuf, "    <param name=\"gaussian mean\">%g</param>\r\n", histo.fit[3]);
        zprintstr(linebuf, compress, zs, 0);
        sprintf(linebuf, "    <param name=\"gaussian width\">%g</param>\r\n", histo.fit[4]);
        zprintstr(linebuf, compress, zs, 0);
        sprintf(linebuf, "    <histogram bins=\"%d\">\r\n", GAIN_CAL_BINS);
        zprintstr(linebuf, compress, zs, 0);
        for (i = 0; i < GAIN_CAL_BINS; i++) {
            sprintf(linebuf, "      <bin num=\"%d\" charge=\"%.4f\" count=\"%d\"></bin>\r\n", 
                   i, histo.x_data[i], (int)histo.y_data[i]);
            zprintstr(linebuf, compress, zs, 0);
        }
        sprintf(linebuf, "    </histogram>\r\n");
        zprintstr(linebuf, compress, zs, 0);
        sprintf(linebuf, "  </histo>\r\n");
        zprintstr(linebuf, compress, zs, 0);
    }
}

/*--------------------------------------------------------------------*/
int zprintstr(char *str, int compress, z_stream *zs, int last) {

    static char out[ZBUFSIZE];
    static char in[ZBUFSIZE];

    static int outsize = 0;

    if (compress) {
        int flush, err;
        
        /* Copy the input string to the input buffer */
        strcpy(in, str);     
        zs->next_in = in;   
        zs->avail_in = strlen(str);

        flush = last ? Z_FINISH : Z_NO_FLUSH;
        do {
            zs->next_out = out;
            zs->avail_out = sizeof(out);
        
            err = deflate(zs, flush);
            write(1, out, sizeof(out)-zs->avail_out);
            outsize += (sizeof(out)-zs->avail_out);
        } while (zs->avail_out == 0);
    }
    else
        printf("%s", str);    

    return outsize;
}

