/*
 * domcal
 *
 * IceCube DOM front-end calibration application.
 * 
 * John Kelley and Jim Braun
 * UW-Madison, 2004
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
    while ((year < 2004) || (year > 2050)) {
        printf("Enter year (2004-...): ");
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
    /* halPowerDownBase(); */

    /* Set up DACs */
    halWriteDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS, ATWD_SAMPLING_SPEED_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD0_RAMP_TOP, ATWD_RAMP_TOP_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD0_RAMP_RATE, ATWD_RAMP_BIAS_DAC);

    halWriteDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, ATWD_SAMPLING_SPEED_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD1_RAMP_TOP, ATWD_RAMP_TOP_DAC);
    halWriteDAC(DOM_HAL_DAC_ATWD1_RAMP_RATE, ATWD_RAMP_BIAS_DAC);

    halWriteDAC(DOM_HAL_DAC_ATWD_ANALOG_REF, ATWD_ANALOG_REF_DAC);
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, ATWD_PEDESTAL_DAC);   

    /* Make sure pulser is off */
    hal_FPGA_TEST_disable_pulser();

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
    for ( i = 0; i < 4; i++ ) {
        c[offset + i] = *( ( char* )&f + i );
    }
    return 4;
}

/* Routine to write a 2 byte memory image of a short into
 * an array of bytes at the specified offset
 */
int get_bytes_from_short( short s, char *c, int offset ) {
    int i;
    for ( i = 0; i < 2; i++ ) {
        c[offset + i] = *( ( char* )&s + i );
    }
    return 2;
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

/* Writes a value_error struct to binary format given byte array and pos */

int write_value_error( value_error *val_er, char *bin_data, int offset ) {
    int bytes_written = get_bytes_from_float( val_er->value, bin_data, offset );
    bytes_written += get_bytes_from_float( val_er->error, bin_data, offset + 4 );
    return bytes_written;
}

/* Writes a dom_calib srtuct to binary format given byte array and pos */

int write_dom_calib( calib_data *cal, char *bin_data ) {

    int offset = 0;
    int i;

    /* Use version to determine endian-ness */
    short vers = MAJOR_VERSION;
    offset += get_bytes_from_short( vers, bin_data, offset );

    /* Write record length */
    short length = RECORD_LENGTH;
    offset += get_bytes_from_short( length, bin_data, offset );

    /* Write date */
    offset += get_bytes_from_short( cal->day, bin_data, offset );  
    offset += get_bytes_from_short( cal->month, bin_data, offset );
    offset += get_bytes_from_short( cal->year, bin_data, offset );

    /* Write DOM ID */
    char *id = cal->dom_id;
    for ( i = 0; i < 13; i++ ) {
        bin_data[offset] = id[i];
        offset++;
    }

    /* Write Kelvin temperature */
    offset += get_bytes_from_float( cal->temp, bin_data, offset );

    /* Write initial DAC values -- before calibration */
    short *dac = cal->dac_values;
    for ( i = 0; i < 16; i++ ) {
        bin_data[offset] = dac[i];
        offset++;
    }

    /* Write initial ADC values -- before calibration */
    short *adc = cal->adc_values;
    for ( i = 0; i < 16; i++ ) {
        bin_data[offset] = adc[i];
        offset++;
    }

    /* Write FADC calibration data */
    short *fadc = cal->fadc_values;
    for ( i = 0; i < 16; i++ ) {
        bin_data[offset] = fadc[i];
        offset++;
    }

    /* Write FE puser calibration data */
    offset += write_fit( &cal->pulser_calib, bin_data, offset );

    /* Write ATWD gain calibration */
    int j;
    for ( i = 0; i < 3; i++ ) {
        for ( j = 0; j < 3; j++ ) {
            offset += write_fit( &( cal->atwd0_gain_calib[i][j] ), bin_data, offset );
        }
    }
    for ( i = 0; i < 3; i++ ) {
        for ( j = 0; j < 3; j++ ) {
            offset += write_fit( &( cal->atwd1_gain_calib[i][j] ), bin_data, offset );
        }
    }

    /* Write FE amplifier calibration */
    for ( i = 0; i < 3; i++ ) {
        offset += write_value_error( &cal->amplifier_calib[i], bin_data, offset );
    }

    /* Write ATWD sampling speed calibration */
    offset += write_fit( &cal->atwd0_freq_calib, bin_data, offset );
    offset += write_fit( &cal->atwd1_freq_calib, bin_data, offset );

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

    printf("ATWD0 Frequency: m=%.6g b=%.6g r^2=%.6g\r\n",
                   dom_calib.atwd0_freq_calib.slope,
                   dom_calib.atwd0_freq_calib.y_intercept,
                   dom_calib.atwd0_freq_calib.r_squared);

    printf("ATWD1 Frequency: m=%.6g b=%.6g r^2=%.6g\r\n",
                   dom_calib.atwd1_freq_calib.slope,
                   dom_calib.atwd1_freq_calib.y_intercept,
                   dom_calib.atwd1_freq_calib.r_squared);

#endif

    char binary_data[RECORD_LENGTH];

    /* Convert DOM calibration data to binary format */
    if ( !( write_dom_calib( &dom_calib, binary_data ) == RECORD_LENGTH ) ) {
        err = FAILED_BINARY_CONVERSION;
    }

    /* Write to flash */
    const char name[11] = "calib_data";
    if ( fisCreate( name, binary_data, RECORD_LENGTH ) != 0 ) {
        err = FAILED_FLASH_WRITE;
    }

    return err;
}

/*---------------------------------------------------------------------------*/

int main(void) {
    
    int err = 0;
    calib_data dom_calib;
    
#ifdef DEBUG
    printf("Hello, world!  This is domcal version %d.%d\r\n", 
           MAJOR_VERSION, MINOR_VERSION);
#endif

    /* Get the date from the user */
    get_date(&dom_calib);
    
    /* Initialize DOM state: DACs, HV setting, pulser, etc. */
    init_dom();
    
    /* Record DOM state, etc. */
    record_state(&dom_calib);

    /* Calibration modules:
     *  - pulser calibration
     *  - atwd calibration
     *  - amplifier calibration
     *  - sampling speed calibration
     */
    /* FIX ME: return real error codes or something */
    pulser_cal(&dom_calib);
    atwd_cal(&dom_calib);
    amp_cal(&dom_calib);
    atwd_freq_cal(&dom_calib);
    
    /* Write calibration record to flash */
    save_results(dom_calib);

#ifdef DEBUG
    if (!err) 
        printf("Calibration completed successfully.\r\n");
#endif
   
    /* Reboot the DOM -- DOESN'T SEEM TO WORK */
    hal_FPGA_TEST_request_reboot();
    while (!hal_FPGA_TEST_is_reboot_granted());

    return 0;
}
