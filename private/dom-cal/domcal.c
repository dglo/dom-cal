/*
 * domcal
 *
 * IceCube DOM front-end calibration application.
 * 
 * John Kelley and Jim Braun
 * UW-Madison, 2004-2006
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "zlib.h"

#include "domcal.h"
#include "calUtils.h"
#include "write_xml.h"

/* Include calibration module headers here */
#include "atwd_cal.h"
#include "amp_cal.h"
#include "pulser_cal.h"
#include "atwd_freq_cal.h"
#include "fadc_cal.h"
#include "hv_gain_cal.h"
#include "baseline_cal.h"
#include "hv_amp_cal.h"
#include "transit_cal.h"
#include "discriminator_cal.h"
#include "pmt_discriminator_cal.h"
#include "daq_baseline_cal.h"
#include "delta_t_cal.h"

/*---------------------------------------------------------------------------*/
/* 
 * Get a string from the terminal -- use instead of scanf
 *
 */
static void getstr(char *str) {
   while (1) {
       const int nr = read(0, str, 1);
       if (nr==1) {
           /* CR means end of string */
           if (*str == '\r') {
               *str = 0;
               return;
           }
           else {
               /* Ignore newline */
               if (*str != '\n') {
                   write(1, str, 1);
                   str++;
               }
           }
           
       }
   }
}

/*---------------------------------------------------------------------------*/
/*
 * get_date
 * 
 * Get the date from user to save in results file, since DOM is oblivious to
 * current date.
 *
 */
void get_date(calib_data *dom_calib) {

    short day, month, year, toroid;
    char timestr[7];
    char buf[100];
    int i;

    /* Get year */
    year = month = 0;
    while ((year < 2009) || (year > 2050)) {
        printf("Enter year (2009-...): ");
        fflush(stdout);    
        getstr(buf);
        year = atoi(buf);
        printf("\r\n");
    }
    
    /* Get month */
    while ((month < 1) || (month > 12)) {
        printf("Enter month (1-12): ");
        fflush(stdout);
        getstr(buf);
        month = atoi(buf);
        printf("\r\n");
    }

    /* Get day of the month */
    short day_ok = 0;
    short day_max;
    while (!day_ok) {
        if ((month == 1) || (month == 3) ||
            (month == 5) || (month == 7) ||
            (month == 8) || (month == 10) ||
            (month == 12)) {
            day_max = 31;
        }
        else if (month == 2) {
            /* Presumably this will not be in use in 2100! */
            day_max = 28 + ((year % 4 == 0) ? 1 : 0);
        }
        else {
            day_max = 30;
        }
        
        printf("Enter day (1-%d): ", day_max);
        fflush(stdout);
        getstr(buf);
        day = atoi(buf);
        printf("\r\n");

        day_ok = ((day >= 1) && (day <= day_max));
    }

    /* Get time string */
    printf("Enter time (HHMMSS, <return> for 000000): ");
    fflush(stdout);
    getstr(buf);
    if (strlen(buf) != 6)
        sprintf(timestr, "000000");
    else {
        for (i = 0; i < 6; i++) {
            if (!isdigit(buf[i])) {
                sprintf(timestr, "000000");
                break;
            }
            else 
                timestr[i] = buf[i];
        }
    }        
    printf("\r\n");

    /* Get toroid type */
    printf("Enter toroid type (0 == old, 1 == new): ");
    fflush(stdout);
    getstr(buf);
    toroid = atoi(buf);
    printf("\r\n");

    /* Store results */
    dom_calib->year = year;
    dom_calib->month = month;
    dom_calib->day = day;
    dom_calib->fe_impedance = getFEImpedance(toroid);

    /* Extract hour, minute, and second */
    char timeBit[3];
    strncpy(timeBit, &(timestr[0]), 2);
    dom_calib->hour = atoi(timeBit);
    strncpy(timeBit, &(timestr[2]), 2);
    dom_calib->minute = atoi(timeBit);
    strncpy(timeBit, &(timestr[4]), 2);
    dom_calib->second = atoi(timeBit);

    if ((dom_calib->hour < 0) || (dom_calib->hour > 24))
        dom_calib->hour = 0;
    if ((dom_calib->minute < 0) || (dom_calib->minute > 59))
        dom_calib->minute = 0;
    /* Java client supports leap seconds! */
    if ((dom_calib->second < 0) || (dom_calib->second > 60))
        dom_calib->second = 0;
}

/*---------------------------------------------------------------------------*/

void init_dom(void) {

    /* Make sure HV is off */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halDisablePMT_HV();
#else
    halDisableBaseHV();
    halPowerDownBase();
#endif
    halUSleep(250000);

    /* Set up DACs */
    halWriteDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS, ATWD_SAMPLING_SPEED_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD0_RAMP_TOP, ATWD_RAMP_TOP_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD0_RAMP_RATE, ATWD_RAMP_BIAS_DAC);

    halWriteDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, ATWD_SAMPLING_SPEED_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD1_RAMP_TOP, ATWD_RAMP_TOP_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD1_RAMP_RATE, ATWD_RAMP_BIAS_DAC);

    halWriteDAC(DOM_HAL_DAC_ATWD_ANALOG_REF, ATWD_ANALOG_REF_DAC);
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, ATWD_PEDESTAL_DAC);   

    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, FAST_ADC_REF);

    halWriteDAC(DOM_HAL_DAC_MUX_BIAS, MUX_BIAS_DAC);   

    /* Make sure pulser is off */
    hal_FPGA_TEST_disable_pulser();

    /* Set disc value for gain cal */
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, DISC_DAC_INIT);

    halUSleep(DAC_SET_WAIT);
}


/*---------------------------------------------------------------------------*/
/* 
 * record_state
 *
 * Records the state of the DOM before data taking -- all DAC and ADC settings,
 * temperature, etc.
 *
 */
void record_state(calib_data *dom_calib) {

    int i;

    /* Read ID */
    strcpy(dom_calib->dom_id, halGetBoardID());

    /* Read DACs */
    for (i = 0; i < 16; i++)
        dom_calib->dac_values[i] = halReadDAC(i);

    /* Read ADCs */
    for (i = 0; i < 24; i++)
        dom_calib->adc_values[i] = halReadADC(i);

    /* Read temperature and convert to K */
    dom_calib->temp = temp2K(halReadTemp());
}

/*---------------------------------------------------------------------------*/

int main(void) {
    
    calib_data dom_calib;
    char buf[100];
    int doHVCal, iterHVGain;
    doHVCal = iterHVGain = 0;

#ifdef DEBUG
    printf("Welcome to domcal version %d.%d.%d\r\n", 
           MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
#endif

    /* Get the date and time from the user */
    /* Toroid query is also in here right now */
    get_date(&dom_calib);
    
    /* Ask user if they want an HV calibration */
    printf("Do you want to perform the HV portion of the calibration (y/n)? ");
    fflush(stdout);
    getstr(buf);
    printf("\r\n");

    /* Sometimes when telnetting there is an extraneous newline */
    doHVCal = ((buf[0] == 'y') || (buf[0] == 'Y') ||
               (buf[0] == '\n' && ((buf[1] == 'y') || (buf[1] == 'Y'))));

    if (doHVCal) {
#ifdef DEBUG
        printf("*** HIGH VOLTAGE WILL BE ACTIVATED ***\r\n");
#endif
    }
    dom_calib.hv_gain_valid = 0;
    dom_calib.hv_baselines_valid = 0;
    dom_calib.daq_baselines_valid = 0;
    dom_calib.transit_calib_valid = 0;
    dom_calib.max_hv = 0;

    /* Query user about multi-iteration runs */
    if (doHVCal) {
        printf("Do you want to iterate the gain/HV calibration (y/n)? ");
        fflush(stdout);
        getstr(buf);
        printf("\r\n");
        iterHVGain = ((buf[0] == 'y') || (buf[0] == 'Y') ||
                      (buf[0] == '\n' && ((buf[1] == 'y') || (buf[1] == 'Y'))));
        iterHVGain = iterHVGain ? GAIN_CAL_MULTI_ITER : 1;
    }

    /* Get max HV */
    if (doHVCal) {
        printf("Enter maximum HV (0V-2000V): ");
        fflush(stdout);
        getstr(buf);
        dom_calib.max_hv = atoi(buf);
        if (dom_calib.max_hv > 2000) dom_calib.max_hv = 2000;
        if (dom_calib.max_hv < 0) dom_calib.max_hv = 0;
        printf("\r\n");
    }

    /* Init # histos returned and number of HV baselines */
    dom_calib.num_histos    = doHVCal ? (GAIN_CAL_HV_CNT * iterHVGain) : 0;
    dom_calib.num_baselines = 0;
    
    /* Initialize DOM state: DACs, HV setting, pulser, etc. */
    init_dom();
    
    /* Record DOM state, etc. */
    record_state(&dom_calib);

#ifdef DEBUG
    printf("Starting calibration: (v%d.%d.%d)\r\n", 
           MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
#endif

    /* Calibration modules:
     *  - discriminator calibration
     *  - atwd calibration
     *  - baseline calibration
     *  - sampling speed calibration
     *  - amplifier calibration (using only pulser)
     *  - FADC calibration 
     *  - HV baseline calibration
     *  - HV amplifier calibration (using LED)
     *  - ...wait...
     *  - transit time calibration
     *  - HV gain calibration
     *  - DAQ baseline calibration
     *  - ATWD/FADC delta-T calibration
     */
    disc_cal(&dom_calib);
    atwd_cal(&dom_calib);
    baseline_cal(&dom_calib);
    atwd_freq_cal(&dom_calib);
    amp_cal(&dom_calib);
    fadc_cal(&dom_calib);
    if (doHVCal) {
        hv_baseline_cal(&dom_calib);
        hv_amp_cal(&dom_calib);

        /* WAIT ~10min for amp cal to finish on neighboring DOMs */
#ifdef DEBUG
        printf(" Waiting 10m for neighbors to settle...\r\n");
#endif
        int i;
        for (i = 0; i < 600; i++) halUSleep(1000000);

        transit_cal(&dom_calib);
        hv_gain_cal(&dom_calib, iterHVGain);        
        pmt_discriminator_cal(&dom_calib);
        /* Switches out FPGA design */
        daq_baseline_cal(&dom_calib);
    }
    /* Switches out FPGA design */
    delta_t_cal(&dom_calib);

    /* Write calibration record to STDOUT */
    int compress = 0;
    /* XML transmission gets slower with higher throttle */
    int throttle = 0;

    printf("Send compressed XML (y/n)?\r\n");    
    fflush(stdout);        
    getstr(buf);        
    printf("\r\n");
    compress = ((buf[0] == 'y') || (buf[0] == 'Y') ||
                (buf[0] == '\n' && ((buf[1] == 'y') || (buf[1] == 'Y'))));        

    write_xml(&dom_calib, throttle, compress);

    /* Reboot the DOM */
    halUSleep( 250000 );
    halBoardReboot();
    
    /* Should never return */
    return -1;
}
