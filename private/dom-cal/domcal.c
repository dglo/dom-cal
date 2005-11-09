/*
 * domcal
 *
 * IceCube DOM front-end calibration application.
 * 
 * John Kelley and Jim Braun
 * UW-Madison, 2004-2005
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

/* Needed for flash filesystem write routine */
#include "../iceboot/fis.h"

#include "domcal.h"
#include "calUtils.h"

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

/*---------------------------------------------------------------------------*/
/* 
 * Get a string from the terminal -- use instead of scanf
 *
 */
static void getstr(char *str) {
   while (1) {
       const int nr = read(0, str, 1);
       if (nr==1) {
           if (*str == '\r') {
               *str = 0;
               return;
           }
           else {
               write(1, str, 1);
               str++;
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

    short day, month, year;
    char buf[100];

    /* Get year */
    year = month = 0;
    while ((year < 2005) || (year > 2050)) {
        printf("Enter year (2005-...): ");
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

    /* Store results */
    dom_calib->year = year;
    dom_calib->month = month;
    dom_calib->day = day;
}

/*---------------------------------------------------------------------------*/

void init_dom(void) {

    /* Make sure HV is off */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halDisablePMT_HV();
#else
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

    /* Make sure pulser is off */
    hal_FPGA_TEST_disable_pulser();

    /* Set disc value for gain cal */
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, GAIN_CAL_DISC_DAC);

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

/* Routine to write a 4 byte memory image of a float into
 * an array of bytes at the specified offset
 */
int get_bytes_from_float( float f, char *c, int offset ) {
    int i;
    for ( i = 0; i < sizeof( float ); i++ ) {
        c[offset + i] = *( ( char* )&f + i );
    }
    return sizeof( float );
}


/* Routine to write a 4 byte memory image of a in into
 * an array of bytes at the specified offset
 */
int get_bytes_from_int( int d, char *c, int offset ) {
    int i;
    for ( i = 0; i < sizeof( int ); i++ ) {
        c[offset + i] = *( ( char* )&d + i );
    }
    return sizeof( int );
}

/* Routine to write a 2 byte memory image of a short into
 * an array of bytes at the specified offset
 */
int get_bytes_from_short( short s, char *c, int offset ) {
    int i;
    for ( i = 0; i < sizeof( short ); i++ ) {
        c[offset + i] = *( ( char* )&s + i );
    }
    return sizeof( short );
}

/* Writes a linear fit to binary format given byte array and pos */
int write_fit( linear_fit *fit, char *bin_data, int offset ) {
    int bytes_written = get_bytes_from_float( fit->slope, bin_data, offset );
    bytes_written += 
              get_bytes_from_float( fit->y_intercept, bin_data, offset + 4 );
    bytes_written += 
                get_bytes_from_float( fit->r_squared, bin_data, offset + 8 );
    return bytes_written;
}

/* Writes a quadratic fit to binary format given byte array and pos */
int write_quadratic_fit( quadratic_fit *fit, char *bin_data, int offset ) {
    int bytes_written = get_bytes_from_float( fit->c0, bin_data, offset );
    bytes_written += get_bytes_from_float( fit->c1, bin_data, offset + 4 );
    bytes_written += get_bytes_from_float( fit->c2, bin_data, offset + 8 );
    bytes_written += get_bytes_from_float( fit->r_squared, bin_data, offset + 12 );
    return bytes_written;
}

/* Writes a value_error struct to binary format given byte array and pos */

int write_value_error( value_error *val_er, char *bin_data, int offset ) {
    int bytes_written = get_bytes_from_float( val_er->value, bin_data, offset );
    bytes_written += get_bytes_from_float( val_er->error, bin_data, offset + 4 );
    return bytes_written;
}

int write_baseline(float *baseline, char *bin_data, int offset) {
    int bytes_written = get_bytes_from_float(baseline[0], bin_data, offset);
    bytes_written += get_bytes_from_float(baseline[1], bin_data, offset + bytes_written);
    bytes_written += get_bytes_from_float(baseline[2], bin_data, offset + bytes_written);
    return bytes_written;
}

int write_hv_baseline(hv_baselines *hv_baseline, char *bin_data, int offset ) {
    int bytes_written = get_bytes_from_short(hv_baseline->voltage, bin_data, offset);
    bytes_written += write_baseline(hv_baseline->atwd0_hv_baseline, bin_data, offset + bytes_written);
    bytes_written += write_baseline(hv_baseline->atwd1_hv_baseline, bin_data, offset + bytes_written);
    return bytes_written;
}

/* Writes a histogram to binary format */
int write_histogram(hv_histogram *hist, char *bin_data, int offset) {
    int bytes_written = get_bytes_from_short(hist->voltage, bin_data, offset);
    bytes_written += get_bytes_from_float(hist->noise_rate, bin_data, offset + bytes_written);
    short filled = hist->is_filled;
    bytes_written += get_bytes_from_short(filled, bin_data, offset + bytes_written);
    bytes_written += get_bytes_from_short(filled ? hist->convergent : 0, bin_data, offset + bytes_written);
    int i;
    for (i = 0; i < 5; i++) {
        bytes_written += get_bytes_from_float(filled ? hist->fit[i] : 0.0, bin_data, offset + bytes_written);
    }
    bytes_written += get_bytes_from_short(hist->bin_count, bin_data, offset + bytes_written);
    for (i = 0; i < hist->bin_count; i++) {
        bytes_written += get_bytes_from_float(filled ? hist->x_data[i] : 0.0, bin_data, offset + bytes_written);
        bytes_written += get_bytes_from_float(filled ? hist->y_data[i] : 0.0, bin_data, offset + bytes_written);
    }
    bytes_written += get_bytes_from_float(filled ? hist->pv : 0.0, bin_data, offset + bytes_written);
    return bytes_written;
} 

/* Writes a dom_calib srtuct to binary format given byte array and pos */

int write_dom_calib( calib_data *cal, char *bin_data, short size ) {

    int offset = 0;
    int i;

    /* Use version to determine endian-ness */
    short vers = MAJOR_VERSION;
    offset += get_bytes_from_short( vers, bin_data, offset );

    /* Write record length */
    offset += get_bytes_from_short( size, bin_data, offset );

    /* Write date */
    offset += get_bytes_from_short( cal->day, bin_data, offset );  
    offset += get_bytes_from_short( cal->month, bin_data, offset );
    offset += get_bytes_from_short( cal->year, bin_data, offset );
    
    /* Add padding */
    short z = 0;
    offset += get_bytes_from_short( z, bin_data, offset );

    /* Write DOM ID, as true hex value (not ASCII) */
    char *id = cal->dom_id;
    char id_lo[9] = "beefcafe";
    char id_hi[5] = "dead";
    unsigned int val_lo, val_hi;

    strncpy(id_hi, id, 4);
    strncpy(id_lo, &(id[4]), 8);

    val_lo = (unsigned int) strtoul(id_lo, NULL, 16);
    val_hi = (unsigned int) strtoul(id_hi, NULL, 16);

    offset += get_bytes_from_int( val_hi, bin_data, offset);
    offset += get_bytes_from_int( val_lo, bin_data, offset);

    /* Write Kelvin temperature */
    offset += get_bytes_from_float( cal->temp, bin_data, offset );

    /* Write initial DAC values -- before calibration */
    short *dac = cal->dac_values;
    for ( i = 0; i < 16; i++ ) {
        offset += get_bytes_from_short( dac[i], bin_data, offset );
    }

    /* Write initial ADC values -- before calibration */
    short *adc = cal->adc_values;
    for ( i = 0; i < 24; i++ ) {
        offset += get_bytes_from_short( adc[i], bin_data, offset );
    }

    /* Write FADC calibration data */
    offset += write_fit( &cal->fadc_baseline, bin_data, offset );
    offset += write_value_error( &cal->fadc_gain, bin_data, offset );

    /* Write FE puser calibration data */
    offset += write_fit( &cal->pulser_calib, bin_data, offset );

    /* Write ATWD gain calibration */
    int j;
    for ( i = 0; i < 3; i++ ) {
        for ( j = 0; j < 128; j++ ) {
            offset += write_fit( &( cal->atwd0_gain_calib[i][j] ), bin_data, offset );
        }
    }
    for ( i = 0; i < 3; i++ ) {
        for ( j = 0; j < 128; j++ ) {
            offset += write_fit( &( cal->atwd1_gain_calib[i][j] ), bin_data, offset );
        }
    }

    /* Write FE amplifier calibration */
    for ( i = 0; i < 3; i++ ) {
        offset += write_value_error( &cal->amplifier_calib[i], bin_data, offset );
    }

    /* Write ATWD sampling speed calibration */
    offset += write_quadratic_fit( &cal->atwd0_freq_calib, bin_data, offset );
    offset += write_quadratic_fit( &cal->atwd1_freq_calib, bin_data, offset );

    /* Write baseline data */
    offset += write_baseline(cal->atwd0_baseline, bin_data, offset);
    offset += write_baseline(cal->atwd1_baseline, bin_data, offset);

    /* Write transit cal isValid */
    offset += get_bytes_from_short( cal->transit_calib_valid, bin_data, offset );

    /* Write transit time data if necessary */
    if (cal->transit_calib_valid) {
        offset += get_bytes_from_short( cal->transit_calib_points, bin_data, offset );
        offset += write_fit(&cal->transit_calib, bin_data, offset);
    }

    /* Write number of histos */
    offset += get_bytes_from_short(cal->num_histos, bin_data, offset );

    /* Write hv baselines valid */
    offset += get_bytes_from_short( cal->hv_baselines_valid, bin_data, offset );

    /* Write baselines if necessary */
    if (cal->hv_baselines_valid) {
        for (i = 0; i < cal->num_histos; i++) {
            offset += write_hv_baseline(&cal->baseline_data[i], bin_data, offset);
        }
    }

    /* Write HV gain cal isValid */
    offset += get_bytes_from_short( cal->hv_gain_valid, bin_data, offset );

    /* Write each histo and baseline */
    for (i = 0; i < cal->num_histos; i++) {
        offset += write_histogram(&cal->histogram_data[i], bin_data, offset);
    }

    /* Write HV gain cal data if necessary */
    if ( cal->hv_gain_valid ) {
        
        /* Write HV gain fit */
        offset += write_fit( &cal->hv_gain_calib, bin_data, offset );

    }

    return offset;
}

/*---------------------------------------------------------------------------*/
/*
 * save_results
 *
 * Save calibration results to the flash filesystem.
 *
 */

int save_results(calib_data dom_calib) {

    int i, ch, bin;
    int err = 0;

    /* FIX ME: For now, just print everything out */
#ifdef DEBUG
    printf("ID: %s\r\n", dom_calib.dom_id);

    for (i = 0; i < 16; i++)
        printf("DAC %d: %d\r\n", i, dom_calib.dac_values[i]);

    for (i = 0; i < 24; i++)
        printf("ADC %d: %d\r\n", i, dom_calib.adc_values[i]);

    printf("Temp: %.1f\r\n", dom_calib.temp);

    printf("Pulser: m=%.6g b=%.6g r^2=%.6g\r\n",
                   dom_calib.pulser_calib.slope,
                   dom_calib.pulser_calib.y_intercept,
                   dom_calib.pulser_calib.r_squared);

    for(ch = 0; ch < 3; ch++)
        for(bin = 0; bin < 128; bin++)
            printf("ATWD0 Ch %d Bin %d Fit: m=%.6g b=%.6g r^2=%.6g\r\n",
                   ch, bin, dom_calib.atwd0_gain_calib[ch][bin].slope,
                   dom_calib.atwd0_gain_calib[ch][bin].y_intercept,
                   dom_calib.atwd0_gain_calib[ch][bin].r_squared);

    for(ch = 0; ch < 3; ch++)
        for(bin = 0; bin < 128; bin++)
            printf("ATWD1 Ch %d Bin %d Fit: m=%.6g b=%.6g r^2=%.6g\r\n",
                   ch, bin, dom_calib.atwd1_gain_calib[ch][bin].slope,
                   dom_calib.atwd1_gain_calib[ch][bin].y_intercept,
                   dom_calib.atwd1_gain_calib[ch][bin].r_squared);

    for(ch = 0; ch < 3; ch++)
        printf("Channel %d gain=%.6g error=%.6g\r\n", ch,
               dom_calib.amplifier_calib[ch].value,
               dom_calib.amplifier_calib[ch].error);

    printf("ATWD0 Frequency: c0=%.6g c1=%.6g c2=%.6g r^2=%.6g\r\n",
                   dom_calib.atwd0_freq_calib.c0,
                   dom_calib.atwd0_freq_calib.c1,
                   dom_calib.atwd0_freq_calib.c2,
                   dom_calib.atwd0_freq_calib.r_squared);

    printf("ATWD1 Frequency: c0=%.6g c1=%.6g c2=%.6g r^2=%.6g\r\n",
                   dom_calib.atwd1_freq_calib.c0,
                   dom_calib.atwd1_freq_calib.c1,
                   dom_calib.atwd1_freq_calib.c2,
                   dom_calib.atwd1_freq_calib.r_squared);

    printf("FADC calibration: baseline fit m=%.6g b=%.6g r^2=%.6g\r\n",
           dom_calib.fadc_baseline.slope,
           dom_calib.fadc_baseline.y_intercept,
           dom_calib.fadc_baseline.r_squared);

    printf("FADC calibration: gain (V/tick)=%.6g error=%.6g\r\n",
           dom_calib.fadc_gain.value, dom_calib.fadc_gain.error);

    if (dom_calib.hv_gain_valid) {
        printf("HV Gain: m=%.6g b=%.6g r^2=%.6g\r\n",
               dom_calib.hv_gain_calib.slope,
               dom_calib.hv_gain_calib.y_intercept,
               dom_calib.hv_gain_calib.r_squared);
    }

#endif

    /* Calculate record length */
    int r_size = DEFAULT_RECORD_LENGTH;
    if ( dom_calib.hv_gain_valid )
        r_size += 12; //log-log fit

    r_size += dom_calib.num_histos * GAIN_CAL_BINS * 8; //histos
    r_size += dom_calib.num_histos * 20; //fits;
    r_size += dom_calib.num_histos * 2; //voltages;
    r_size += dom_calib.num_histos * 2; //bin counts;
    r_size += dom_calib.num_histos * 2; //convergent bits
    r_size += dom_calib.num_histos * 4; //PV data
    r_size += dom_calib.num_histos * 4; //Noise rate
    r_size += dom_calib.num_histos * 2; //is_filled flag
    
    r_size += 2; //hv_baselines_valid
    if (dom_calib.hv_baselines_valid) {
        r_size += dom_calib.num_histos * 2 * 3 * 4; //hv_baselines
        r_size += dom_calib.num_histos * 2; //baseline voltages
    }

    r_size += 2; //transit_calib_valid
    if (dom_calib.transit_calib_valid) {
        /* Number of points and linear fit */
        r_size +=  2;
        r_size += 12;
    }

    r_size += 2 * 3 * 4; //baselines

    char binary_data[r_size];

    /* Convert DOM calibration data to binary format */
    int b_write = write_dom_calib( &dom_calib, binary_data, r_size );
#ifdef DEBUG
    printf( "Writing %d bytes to flash....", b_write );
#endif
    if ( !( b_write == r_size ) ) {
        err = FAILED_BINARY_CONVERSION;
    }

    /* Write to flash */
    const char name[11] = "calib_data";
    if ( fisCreate( name, binary_data, r_size ) != 0 ) {
        err = FAILED_FLASH_WRITE;
    }

    return err;
}

/*---------------------------------------------------------------------------*/

int main(void) {
    
    int err = 0;
    calib_data dom_calib;
    char buf[100];
    int doHVCal;

#ifdef DEBUG
    printf("Welcome to domcal version %d.%d\r\n", 
           MAJOR_VERSION, MINOR_VERSION);
#endif

    /* Get the date from the user */
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
    dom_calib.transit_calib_valid = 0;

    /* Init # histos returned */
    dom_calib.num_histos = doHVCal ? GAIN_CAL_HV_CNT : 0;

    /* Initialize DOM state: DACs, HV setting, pulser, etc. */
    init_dom();
    
    /* Record DOM state, etc. */
    record_state(&dom_calib);

    /* Calibration modules:
     *  - pulser calibration
     *  - atwd calibration
     *  - baseline calibration
     *  - sampling speed calibration
     *  - amplifier calibration (using only pulser)
     *  - FADC calibration 
     *  - transit time calibration
     *  - HV baseline calibration
     *  - HV amplifier calibration (using LED)
     *  - HV gain calibration
     */
    /* FIX ME: return real error codes */
    pulser_cal(&dom_calib);
    atwd_cal(&dom_calib);
    baseline_cal(&dom_calib);
    atwd_freq_cal(&dom_calib);
    amp_cal(&dom_calib);
    fadc_cal(&dom_calib);
    if (doHVCal) {
        transit_cal(&dom_calib);
        hv_baseline_cal(&dom_calib);
        hv_amp_cal(&dom_calib);
        hv_gain_cal(&dom_calib);
    }

    /* Write calibration record to flash */
    int save_ret = save_results( dom_calib );
    if ( !save_ret ) {
#ifdef DEBUG       
        printf( "done.\r\n" );
#endif
    } else {
#ifdef DEBUG       
        printf( "FAILED (error %d)\r\n", save_ret );
#endif
        err = save_ret;
    }    

#ifdef DEBUG
    if (!err) 
        printf("Calibration completed successfully.  Rebooting...\r\n");
#endif
   
    /* Reboot the DOM */
    halUSleep( 250000 );
    halBoardReboot();
    
    /* Should never return */
    return -1;
}
