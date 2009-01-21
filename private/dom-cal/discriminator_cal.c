/*
 * pulser_cal
 *
 * Analog FE pulser calibration routine -- Determine pulser amplitude
 * relationship to DAC setting using SPE discriminator
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"

#include "domcal.h"
#include "discriminator_cal.h"
#include "calUtils.h"

/* Routine to find pulser DAC where 50% of pulses trigger discriminator */
int pulser_loop(int *pulser_dac, int mpe) {

    int max = 1023;
    int min = 0;
    int count = 0;

    /* Binary search until max and min differ by one bin */
    for (count = 0; max - min > 1; count++) {

        *pulser_dac = (max + min) / 2;

        /* Set pulser amplitude and wait */
        halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, *pulser_dac);
        halUSleep(DAC_SET_WAIT);

        /* Retreive current discriminator rate */
        int rate = mpe ? hal_FPGA_TEST_get_mpe_rate() :
                             hal_FPGA_TEST_get_spe_rate();
        
        /* Search for amplitude where half (~3900) pulses cross thresh */
        if (rate < 3900) {
            min = *pulser_dac;
        } else {
            max = *pulser_dac;
        }
    }

    return 0;
}

    

/*---------------------------------------------------------------------------*/
int disc_cal(calib_data *dom_calib) {

    #ifdef DEBUG
    printf("Performing discriminator calibration...\r\n");
    #endif

    /* Record current DAC values */
    int old_pedestal_value = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    int old_spe_disc_value = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);
    int old_mpe_disc_value = halReadDAC(DOM_HAL_DAC_MULTIPLE_SPE_THRESH);
    int old_pulser_amplitude = halReadDAC(DOM_HAL_DAC_INTERNAL_PULSER);

    /* Turn pulser on */
    hal_FPGA_TEST_enable_pulser();

    /* Pulser charges */
    float pulser_spe_charge_data[DISC_CAL_CNT];
    float pulser_mpe_charge_data[DISC_CAL_CNT];
    
    /* Array of disc DAC settings */
    float spe_disc_data[DISC_CAL_CNT];
    float mpe_disc_data[DISC_CAL_CNT];

    /* Set bias value */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, PEDESTAL_VALUE);
    halUSleep(DAC_SET_WAIT);
    
    /* Turn pulser on */
    hal_FPGA_TEST_enable_pulser();

    int i, pulser_dac;
    for (i = 0; i < DISC_CAL_CNT; i++) {
        
        /* Set discriminator value */
        spe_disc_data[i] = DISC_CAL_MIN + i * SPE_DISC_CAL_INC;
        mpe_disc_data[i] = DISC_CAL_MIN + i * MPE_DISC_CAL_INC;
        halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, (short)spe_disc_data[i]);
        halWriteDAC(DOM_HAL_DAC_MULTIPLE_SPE_THRESH, (short)mpe_disc_data[i]);

        /* Find pulser charge (DAC) corresponding to spe discriminator value */
        if (pulser_loop(&pulser_dac, 0)) return -1;

        /* Convert pulser amplitude to charge */
        pulser_spe_charge_data[i] = pulserDAC2Q(pulser_dac);

        /* Do same for MPE disc */
        if (pulser_loop(&pulser_dac, 1)) return -1;

        /* Convert pulser amplitude to charge */
        pulser_mpe_charge_data[i] = pulserDAC2Q(pulser_dac);

        #ifdef DEBUG
        printf("Got disc point: SPE DAC: %f SPE Q: %f MPE DAC: %f MPE Q: %f\r\n",
                                  spe_disc_data[i], pulser_spe_charge_data[i],
                                  mpe_disc_data[i], pulser_mpe_charge_data[i]);
        #endif
    }

    /* Do linear fit, x-axis pulser amplitude, y-axis SPE thresh */
    linearFitFloat(spe_disc_data, pulser_spe_charge_data,
                         DISC_CAL_CNT, &dom_calib->spe_disc_calib);
    linearFitFloat(mpe_disc_data, pulser_mpe_charge_data,
                         DISC_CAL_CNT, &dom_calib->mpe_disc_calib);

    /* Refine each fit */
    int vld_cnt = DISC_CAL_CNT;
    refineLinearFit(spe_disc_data, pulser_spe_charge_data, 
                   &vld_cnt, NULL, &(dom_calib->spe_disc_calib),
                   DISC_CAL_MIN_R2, DISC_CAL_MIN_R2_PTS);
#ifdef DEBUG
    if (vld_cnt != DISC_CAL_CNT) printf("Using only %d pts for fit\n", vld_cnt);
#endif

    vld_cnt = DISC_CAL_CNT;
    refineLinearFit(mpe_disc_data, pulser_mpe_charge_data, 
                   &vld_cnt, NULL, &(dom_calib->mpe_disc_calib),
                   DISC_CAL_MIN_R2, DISC_CAL_MIN_R2_PTS);
#ifdef DEBUG
    if (vld_cnt != DISC_CAL_CNT) printf("Using only %d pts for fit\n", vld_cnt);
#endif

    /* Restore DAC values */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, old_pedestal_value);
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, old_spe_disc_value);
    halWriteDAC(DOM_HAL_DAC_MULTIPLE_SPE_THRESH, old_mpe_disc_value);
    halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, old_pulser_amplitude);

    /* Turn pulser off */
    hal_FPGA_TEST_disable_pulser();
    
    return 0;
}
