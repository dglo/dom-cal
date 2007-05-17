
/*
 * Write the DOMCal XML file to STDOUT.  Captured by surface client.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "hv_gain_cal.h"
#include "write_xml.h"

/*--------------------------------------------------------------------*/

unsigned int write_xml(calib_data *dom_calib, int retx) {
    
    unsigned int crc = 0;
    int i, atwd, ch, bin;

    /* Buffer for CRC checks */
    char linebuf[1000];

    /* Opening tag and version information */
    char version[10];
    sprintf(version, "%d.%d.%d", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    sprintf(linebuf, "<domcal version=\"%s\">\r\n", version);
    crc_printstr(&crc, linebuf);

    /* Vitals */
    sprintf(linebuf, "  <date>%d-%d-%d</date>\r\n", dom_calib->day, dom_calib->month, dom_calib->year);
    crc_printstr(&crc, linebuf);
    sprintf(linebuf, "  <time>%02d:%02d:%02d</time>\r\n", dom_calib->hour, dom_calib->minute, dom_calib->second);
    crc_printstr(&crc, linebuf);
    sprintf(linebuf, "  <domid>%s</domid>\r\n", dom_calib->dom_id);    
    crc_printstr(&crc, linebuf);
    sprintf(linebuf, "  <temperature format=\"Kelvin\">%.1f</temperature>\r\n", dom_calib->temp);
    crc_printstr(&crc, linebuf);

    /* DAC and ADC values */
    for (i = 0; i < 16; i++ ) {
        sprintf(linebuf, "  <dac channel=\"%d\">%d</dac>\r\n", i, dom_calib->dac_values[i]);
        crc_printstr(&crc, linebuf);
        halUSleep(10000 * retx);
    }
    for (i = 0; i < 24; i++ ) {
        sprintf(linebuf, "  <adc channel=\"%d\">%d</adc>\r\n", i, dom_calib->adc_values[i]);
        crc_printstr(&crc, linebuf);
        halUSleep(10000 * retx);
    }

    sprintf(linebuf, "  <frontEndImpedance format=\"Ohms\">%.1f</frontEndImpedance>\r\n", dom_calib->fe_impedance);
    crc_printstr(&crc, linebuf);

    /* SPE discriminator calibration */
    sprintf(linebuf, "  <discriminator id=\"spe\">\r\n");
    crc_printstr(&crc, linebuf);
    write_linear_fit(&crc, dom_calib->spe_disc_calib);    
    sprintf(linebuf, "  </discriminator>\r\n");
    crc_printstr(&crc, linebuf);

    /* MPE discriminator calibration */
    sprintf(linebuf, "  <discriminator id=\"mpe\">\r\n");
    crc_printstr(&crc, linebuf);
    write_linear_fit(&crc, dom_calib->mpe_disc_calib);
    sprintf(linebuf, "  </discriminator>\r\n");
    crc_printstr(&crc, linebuf);

    /* ATWD bin-by-bin calibration */
    for ( atwd = 0; atwd < 2; atwd++) {
        for ( ch = 0; ch < 3; ch++ ) {
            for ( bin = 0; bin < 128; bin++ ) {
                sprintf(linebuf, "  <atwd id=\"%d\" channel=\"%d\" bin=\"%d\">\r\n", atwd, ch, bin);
                crc_printstr(&crc, linebuf);
                if (atwd == 0)
                    write_linear_fit(&crc, dom_calib->atwd0_gain_calib[ch][bin]);
                else
                    write_linear_fit(&crc, dom_calib->atwd1_gain_calib[ch][bin]);
                sprintf(linebuf, "  </atwd>\r\n");
                crc_printstr(&crc, linebuf);
                halUSleep(10000 * retx);
            }
        }
    }

    /* FADC baseline calibration */
    sprintf(linebuf, "  <fadc_baseline>\r\n");
    crc_printstr(&crc, linebuf);
    write_linear_fit(&crc, dom_calib->fadc_baseline);
    sprintf(linebuf, "  </fadc_baseline>\r\n");
    crc_printstr(&crc, linebuf);

    /* FADC gain calibration */
    sprintf(linebuf, "  <fadc_gain>\r\n");
    crc_printstr(&crc, linebuf);
    sprintf(linebuf, "    <gain error=\"%g\">%g</gain>\r\n", dom_calib->fadc_gain.error, dom_calib->fadc_gain.value);
    crc_printstr(&crc, linebuf);
    sprintf(linebuf, "  </fadc_gain>\r\n");
    crc_printstr(&crc, linebuf);

    /* FADC time offset calibration */
    sprintf(linebuf, "  <fadc_delta_t>\r\n");
    crc_printstr(&crc, linebuf);
    sprintf(linebuf, "    <delta_t error=\"%g\">%g</delta_t>\r\n", dom_calib->fadc_delta_t.error, dom_calib->fadc_delta_t.value);
    crc_printstr(&crc, linebuf);
    sprintf(linebuf, "  </fadc_delta_t>\r\n");
    crc_printstr(&crc, linebuf);

    /* ATWD channel gain calibration */
    for ( ch = 0; ch < 3; ch++) {
        sprintf(linebuf, "  <amplifier channel=\"%d\">\r\n", ch);
        crc_printstr(&crc, linebuf);
        sprintf(linebuf, "    <gain error=\"%g\">%g</gain>\r\n", 
               dom_calib->amplifier_calib[ch].error,
               dom_calib->amplifier_calib[ch].value);
        crc_printstr(&crc, linebuf);
        sprintf(linebuf, "  </amplifier>\r\n");
        crc_printstr(&crc, linebuf);
        halUSleep(100000 * retx);
    }

    /* ATWD sampling frequency calibration */
    for ( atwd = 0; atwd < 2; atwd++) {
        sprintf(linebuf, "  <atwdfreq atwd=\"%d\">\r\n", atwd);
        crc_printstr(&crc, linebuf);
        if (atwd == 0)
            write_quadratic_fit(&crc, dom_calib->atwd0_freq_calib);
        else
            write_quadratic_fit(&crc, dom_calib->atwd1_freq_calib);
        sprintf(linebuf, "  </atwdfreq>\r\n");
        crc_printstr(&crc, linebuf);
    }

    /* HV off baseline calibration */
    sprintf(linebuf, "  <baseline voltage=\"0\">\r\n");
    crc_printstr(&crc, linebuf);
    for ( atwd = 0; atwd < 2; atwd++) {
        for ( ch = 0; ch < 3; ch++) {
            if (atwd == 0) {
                sprintf(linebuf, "    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                       atwd, ch, dom_calib->atwd0_baseline[ch]);
                crc_printstr(&crc, linebuf);
            }
            else {
                sprintf(linebuf, "    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                       atwd, ch, dom_calib->atwd1_baseline[ch]);
                crc_printstr(&crc, linebuf);
            }
            halUSleep(100000 * retx);
        }
    }
    sprintf(linebuf, "  </baseline>\r\n");
    crc_printstr(&crc, linebuf);

    /* Domapp FPGA baseline calibration */
    if (dom_calib->daq_baselines_valid) {
        sprintf(linebuf, "  <daq_baseline>\r\n");
        crc_printstr(&crc, linebuf);
        for ( atwd = 0; atwd < 2; atwd++) {
            for ( ch = 0; ch < 3; ch++){
                for (bin = 0; bin < 128; bin++) {
                    sprintf(linebuf, "   <waveform atwd=\"%d\" channel=\"%d\" bin=\"%d\">", atwd, ch, bin);
                    crc_printstr(&crc, linebuf);
                    if (atwd == 0) {
                        sprintf(linebuf, "%g</waveform>\r\n", dom_calib->atwd0_daq_baseline_wf[ch][bin]);
                        crc_printstr(&crc, linebuf);
                    }
                    else {
                        sprintf(linebuf, "%g</waveform>\r\n", dom_calib->atwd1_daq_baseline_wf[ch][bin]);
                        crc_printstr(&crc, linebuf);
                    }
                    halUSleep(10000 * retx);
                }
            }
        }
        sprintf(linebuf, "  </daq_baseline>\r\n");
        crc_printstr(&crc, linebuf);
    }

    /* Transit time calibration */
    if (dom_calib->transit_calib_valid) {
        sprintf(linebuf, "  <pmtTransitTime num_pts=\"%d\">\r\n", dom_calib->transit_calib_points);
        crc_printstr(&crc, linebuf);
        write_linear_fit(&crc, dom_calib->transit_calib);
        sprintf(linebuf, "  </pmtTransitTime>\r\n");
        crc_printstr(&crc, linebuf);
    }

    /* HV / Gain calibration */
    if (dom_calib->hv_gain_valid) {            
        sprintf(linebuf, "  <hvGainCal>\r\n");
        crc_printstr(&crc, linebuf);
        write_linear_fit(&crc, dom_calib->hv_gain_calib);
        sprintf(linebuf, "  </hvGainCal>\r\n");
        crc_printstr(&crc, linebuf);
    }
    halUSleep(1000000 * retx);

    /* Baseline measurements at various HV settings */
    if (dom_calib->hv_baselines_valid) {
        for (i = 0; i < dom_calib->num_baselines; i++) {
            hv_baselines *base = &(dom_calib->baseline_data[i]);
            sprintf(linebuf, "  <baseline voltage=\"%d\">\r\n", base->voltage);
            crc_printstr(&crc, linebuf);
            for ( atwd = 0; atwd < 2; atwd++) {
                for ( ch = 0; ch < 3; ch++) {
                    if (atwd == 0) {
                        sprintf(linebuf, "    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                               atwd, ch, base->atwd0_hv_baseline[ch]);
                        crc_printstr(&crc, linebuf);
                    }
                    else {
                        sprintf(linebuf, "    <base atwd=\"%d\" channel=\"%d\" value=\"%g\"/>\r\n", 
                               atwd, ch, base->atwd1_hv_baseline[ch]);
                        crc_printstr(&crc, linebuf);
                    }
                }                
                halUSleep(10000 * retx);           
            }
            sprintf(linebuf, "  </baseline>\r\n");
            crc_printstr(&crc, linebuf);
        }
    }

    /* HV histograms at various voltages, including fit parameters */
    for (i = 0; i < dom_calib->num_histos; i++) {
        write_histogram(&crc, dom_calib->histogram_data[i]);
        halUSleep(500000 * retx);
    }
    sprintf(linebuf, "</domcal>\r\n");
    crc_printstr(&crc, linebuf);

    halUSleep(1000000 * retx);

    return crc;
}

/*--------------------------------------------------------------------*/
/* 
 * Write a linear fit element to STDOUT
 */
void write_linear_fit(unsigned int *pCrc, linear_fit fit) {

    char buf[2000];

    sprintf(buf, "    <fit model=\"linear\">\r\n");
    sprintf(buf, "%s      <param name=\"slope\">%g</param>\r\n", buf, fit.slope);
    sprintf(buf, "%s      <param name=\"intercept\">%g</param>\r\n", buf, fit.y_intercept);
    sprintf(buf, "%s      <regression-coeff>%g</regression-coeff>\r\n", buf, fit.r_squared);
    sprintf(buf, "%s    </fit>\r\n", buf);
    
    crc_printstr(pCrc, buf);
}

/*--------------------------------------------------------------------*/
/*
 * Write a quadratic fit element to STDOUT
 */
void write_quadratic_fit(unsigned int *pCrc, quadratic_fit fit) {
    char buf[2000];

    sprintf(buf, "    <fit model=\"quadratic\">\r\n");
    sprintf(buf, "%s      <param name=\"c0\">%g</param>\r\n", buf, fit.c0);
    sprintf(buf, "%s      <param name=\"c1\">%g</param>\r\n", buf, fit.c1);
    sprintf(buf, "%s      <param name=\"c2\">%g</param>\r\n", buf, fit.c2);
    sprintf(buf, "%s      <regression-coeff>%g</regression-coeff>\r\n", buf, fit.r_squared);
    sprintf(buf, "%s    </fit>\r\n", buf);

    crc_printstr(pCrc, buf);
}

/*--------------------------------------------------------------------*/
/*
 * Write histogram data, fit parameters, etc. at a given voltage
 */
void write_histogram(unsigned int *pCrc, hv_histogram histo) {

    int i;
    char linebuf[1000];

    /* Don't write anything if histogram is not filled */
    if (histo.is_filled) {
        sprintf(linebuf, "  <histo voltage=\"%d\" convergent=\"%d\" pv=\"%.2f\" noiseRate=\"%g\" isFilled=\"%d\">\r\n", 
               histo.voltage, histo.convergent, histo.pv, histo.noise_rate, histo.is_filled);    
        crc_printstr(pCrc, linebuf);
        sprintf(linebuf, "    <param name=\"exponential amplitude\">%g</param>\r\n", histo.fit[0]);
        crc_printstr(pCrc, linebuf);
        sprintf(linebuf, "    <param name=\"exponential width\">%g</param>\r\n", histo.fit[1]);
        crc_printstr(pCrc, linebuf);
        sprintf(linebuf, "    <param name=\"gaussian amplitude\">%g</param>\r\n", histo.fit[2]);
        crc_printstr(pCrc, linebuf);
        sprintf(linebuf, "    <param name=\"gaussian mean\">%g</param>\r\n", histo.fit[3]);
        crc_printstr(pCrc, linebuf);
        sprintf(linebuf, "    <param name=\"gaussian width\">%g</param>\r\n", histo.fit[4]);
        crc_printstr(pCrc, linebuf);
        sprintf(linebuf, "    <histogram bins=\"%d\">\r\n", GAIN_CAL_BINS);
        crc_printstr(pCrc, linebuf);
        for (i = 0; i < GAIN_CAL_BINS; i++) {
            sprintf(linebuf, "      <bin num=\"%d\" charge=\"%.4f\" count=\"%d\"></bin>\r\n", 
                   i, histo.x_data[i], (int)histo.y_data[i]);
            crc_printstr(pCrc, linebuf);
        }
        sprintf(linebuf, "    </histogram>\r\n");
        crc_printstr(pCrc, linebuf);
        sprintf(linebuf, "  </histo>\r\n");
        crc_printstr(pCrc, linebuf);
    }
}

/*--------------------------------------------------------------------*/
/*
 * CRC functions: CRC32 used in HAL and in IEEE 802.3
 * Must match CRC function as coded in surface client
 */
void crc_printstr(unsigned int *pCrc, char *str) {
    printf("%s", str);
    crc32(pCrc, str, strlen(str));
}

/* 32-bit CRC; on first call set crc value to 0 */
void crc32(unsigned int *pCrc, unsigned char *b, int n) {
   int i;
   for (i=0; i<n; i++) crc32Once(pCrc, b[i]);
}

void crc32Once(unsigned int *pCrc, unsigned char cval) {
   int i;
   const unsigned poly32 = 0x04C11DB7;
   *pCrc ^= cval << 24;
   for (i = 8; i--; )
      *pCrc = *pCrc & 0x80000000 ? (*pCrc << 1) ^ poly32 : *pCrc << 1;
}
