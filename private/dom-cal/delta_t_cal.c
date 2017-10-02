/*
 * delta_t_cal
 *
 * Measure time offset between ATWD A, ATWD B, and FADC features
 * using the front-end pulser.  Uses domapp FPGA, because FADC
 * capture window is different than STF FPGA.
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_FPGA_domapp_regs.h"

#include "domcal.h"
#include "calUtils.h"
#include "zlib.h"
#include "icebootUtils.h"
#include "lmfit.h"
#include "delta_t_cal.h"

/*---------------------------------------------------------------------------*/
/*
 * fadc_pulse
 *
 * Function passed to nonlinear fit routine -- returns value and Jacobian
 * entries for the pulser fadc pulse fit function, y(x) = A*(exp(-(x-x0)/30)+exp((x-x0)/186))^-8
 *
 */
void fadc_pulse(float x, float *a, float *y, float *dyda, int nparam){
	float z, e1, e2, es, es8;
	/* compute intermediate values */
	z = x - a[1];
	e1 = exp(-z/30.0f);
	e2 = exp(z/186.0f);
	es = e1+e2;
	es8 = pow(es,8.0f);
	/* evaluate function */
	*y = a[0]/es8;
	/* evaulate jacobian */
	dyda[0] = 1.0f/es8;
	dyda[1] = (-8.0f*a[0]*(e1/30.0f - e2/186.0f))/(es*es8);
}

/*---------------------------------------------------------------------------*/
/*
 * atwd_new_pulse
 *
 * Function passed to nonlinear fit routine -- returns value and Jacobian
 * entries for the pulser atwd pulse fit function, y(x) = A*(exp(-(x-x0)/5.5)+exp((x-x0)/42))^-8
 * for a DOM with a new torroid
 *
 */
void atwd_new_pulse(float x, float *a, float *y, float *dyda, int nparam){
	float z, e1, e2, es, es8;
	/* compute intermediate values */
	z = x - a[1];
	e1 = exp(-z/5.5f);
	e2 = exp(z/42.0f);
	es = e1+e2;
	es8 = pow(es,8.0f);
	/* evaluate function */
	*y = a[0]/es8;
	/* evaulate jacobian */
	dyda[0] = 1.0f/es8;
	dyda[1] = (-8.0f*a[0]*(e1/5.5f - e2/42.0f))/(es*es8);
}

/*---------------------------------------------------------------------------*/
/*
 * atwd_old_pulse
 *
 * Function passed to nonlinear fit routine -- returns value and Jacobian
 * entries for the pulser atwd pulse fit function, y(x) = A*(exp(-(x-x0)/4.7)+exp((x-x0)/39))^-8
 * for a DOM with an old torroid
 *
 */
void atwd_old_pulse(float x, float *a, float *y, float *dyda, int nparam){
	float z, e1, e2, es, es8;
	/* compute intermediate values */
	z = x - a[1];
	e1 = exp(-z/4.7f);
	e2 = exp(z/39.0f);
	es = e1+e2;
	es8 = pow(es,8.0f);
	/* evaluate function */
	*y = a[0]/es8;
	/* evaulate jacobian */
	dyda[0] = 1.0f/es8;
	dyda[1] = (-8.0f*a[0]*(e1/4.7f - e2/39.0f))/(es*es8);
}

/*---------------------------------------------------------------------------*/
/*
 * pulseFit
 *
 * Manages the nonlinear fit of the pulser pulse waveform
 * xdata and ydata are the x and y coordinates of the points to fit
 * ndata is the number of points in xdata and ydata
 * fit_params is an array (length 2) of fit parameters which must initially contain starting guesses
 * pulse_func is a pointer to the function implmenting the pulse form to be fitted
 *
 */
int pulseFit(float* xdata, float* ydata, int ndata, float* fit_params, void(*pulse_func)(float,float*,float*,float*,int)){
	printf("Beginning non-linear pulse fit with %i data points\n",ndata);
	int err = PULSE_FIT_NO_ERR;
	int iter = 0;
	
	/* Working matrices */
    float **alpha = matrix(PULSE_FIT_PARAMS, PULSE_FIT_PARAMS);
    float **covar = matrix(PULSE_FIT_PARAMS, PULSE_FIT_PARAMS);
	
	/* Chi squared */
    float chisq = 0.0;
	
    /* Must be negative for first iteration */
    float lambda = -1.0;
	
	float *sigma = vector(ndata);
	int i;
    for (i = 0; i < ndata; i++) {
		sigma[i] = 1.0; /* assume std dev of one count for now */
		/* could be more sophisticated and take into account number of samples which were averaged */
    }
	
	/* Fit loop */
    int done = 0;
    int converged = 0;
    float old_chisq = chisq;
    float del_chisq;
	
    while(!done){
		
        /* Loop is done when the max number of iterations is reached */
        /* or when chi-squared has decreased, but only by a small amount */
        /* Decrease can be small absolutely or as a percentage */
		
        iter++;
        del_chisq = old_chisq - chisq;
        converged = (del_chisq > 0) && ((del_chisq < PULSE_FIT_CHISQ_ABS_DONE) ||
                                        (del_chisq / chisq < PULSE_FIT_CHISQ_PCT_DONE));
		
        done = (iter > PULSE_FIT_MAX_ITER) || converged;
        
        /* Setting to zero on final iteration will read out covariance and curvature */
        if(done)
            lambda = 0.0;
		
        /* Save chi-squared */
        old_chisq = chisq;
		
        /* Do an iteration and watch for errors */
        if(lmfit(xdata, ydata, sigma, ndata,
                  fit_params, PULSE_FIT_PARAMS, covar,
                  alpha, &chisq, pulse_func, &lambda)){
			
            done = 1;
            err = PULSE_FIT_ERR_SINGULAR;
#ifdef DEBUG
            printf("SPE fit error: singular matrix\r\n");
#endif
        };
    }
	
	/* free memory */
	free_vector(sigma);
    free_matrix(alpha, PULSE_FIT_PARAMS);
    free_matrix(covar, PULSE_FIT_PARAMS);
	
	/* check results */
	if (converged) {
#ifdef DEBUG
        printf("SPE fit converged successfully\r\n");
#endif
        /* Check that all fit parameters are positive and sane */
        for(i = 0; i < PULSE_FIT_PARAMS; i++){
            if(fit_params[i] <= 0.0)
                err = PULSE_FIT_ERR_BAD_FIT;
        }
    }
    else{
#ifdef DEBUG
        printf("SPE fit error: singular matrix or no convergence!\r\n");
#endif
        err = PULSE_FIT_ERR_NO_CONVERGE;
    }
	
	printf("Finished non-linear pulse fit\n");
	
	printf("Dump of fit input: x y fit value\n");
	for (i = 0; i < ndata; i++) {
		float fit_y;
		float jacobian[2];
		pulse_func(xdata[i],fit_params,&fit_y,jacobian,2);
		printf("\t%f %f %f\n",xdata[i],ydata[i],fit_y);
	}
	
    return err;
}

/* helper functions which know how to compute a bin time depending on whether it is in the atwd or fadc */
float atwd_bin_time(int bin, int start, int stop, float freq){
	return(((stop-bin-1)*1000.0f)/freq);
}

float fadc_bin_time(int bin, int start, int stop, float freq){
	return((bin*1000.0f)/freq);
}

/*---------------------------------------------------------------------------*/
/*
 * guessPulseFitParams
 *
 * Manages the nonlinear fit of the pulser pulse waveform
 * samples is the list of averaged samples
 *    only samples within the range [start_bin,stop_bin) are considered
 * freq is the frequency at which the samples were taken, in MHz
 * bin_time_func is a pointer to the function which know how to compute bin times for the data
 * x is the array to be filled with x values for points over which to do the fit
 * y is the array to be filled with y values for points over which to do the fit
 * n_points is a pointer to the variable which will hold the number of points placed in x and y
 * fit_params is the array (length 2) to be filled with fit parameter guesses
 *
 */
void guessPulseFitParams(float* samples, int start_bin, int stop_bin, float freq, float(*bin_time_func)(int,int,int,float), float integral_scale, float* x, float* y, int* n_points, float* fit_params){
	int bin;
	int max_bin=start_bin;
	float max=samples[max_bin];
	
	/* first find the largest sample and its value */
	for(bin=start_bin; bin<stop_bin; bin++){
		if(samples[bin]>max){
			max_bin = bin;
			max = samples[max_bin];
		}
	}
	
	/* let the threshold be 10% of the maximum */
	float threshold = 0.1f * max;
	/* locate the first bin over the threshold */
	for(bin=start_bin; bin<stop_bin; bin++){
		if(samples[bin]>=threshold)
			break;
	}
	/* if there is one available, take one bin before the first over threshold */
	if(bin>start_bin)
		bin--;
	
	/* set the guess for the second fit parameter, x0, equal to the time of the first bin included */
	fit_params[1] = bin_time_func(bin,start_bin,stop_bin,freq);
	
	/* collect all samples until the waveform falls back below threshold
	 * along the way, compute the sum of all included bins so the integral can be computed */
	*n_points=0;
	float sum=0.0f;
	for(/*omitted*/; bin<stop_bin; bin++){
		x[*n_points] = bin_time_func(bin,start_bin,stop_bin,freq);
		y[*n_points] = samples[bin];
		sum+=y[*n_points];
		(*n_points)++;
		/* stop if the samples fall below threshold, but don't do so on the first sample; it is expected to be below threshold */
		if((samples[bin] < threshold) && *n_points!=1)
			break;
	}
	/* if we were actually iterating anti-chronologically along the waveform, the first bin 
	 * over 10% of maximum we encountered was actually the falling edge
	 * if that happened we would be at the rising edge here, which would have a smaller time, 
	 * and so we replace our estimate of x0 */
	if(bin_time_func(bin,start_bin,stop_bin,freq) < fit_params[1])
		fit_params[1] = bin_time_func(bin,start_bin,stop_bin,freq);
	
	/* set the guess for the first fit parameter, A, by scaling the integral of the waveform */
	fit_params[0] = (sum*1000.0f/freq)/integral_scale;
#ifdef DEBUG
	printf("Guess fit parameters are A=%f, x0=%f\n",fit_params[0],fit_params[1]);
#endif
}

int delta_t_cal(calib_data *dom_calib) {

    int cnt = 128;
    int fadc_cnt = 256;

    int ch, bin;
    int npeds, n_atwd[2];
    
    /* Which atwd to use for time reference */
    /* For a consistent calibration, whether or not this matches transit time calibration ATWD  must be taken into account*/
    short atwd = dom_calib->preferred_atwd;
    
    /* Calculate offset for other atwd */
    short atwd2 = atwd ^ 0x1;

    /* ATWD and FADC baselines */
    float ch0_baseline_wf[2][128];
    float fadc_baseline;

    /* Average FADC waveform */
    float fadc_avg[256];

    /* Average ATWD waveforms */
    float atwd_avg[2][128];

    /* Time offset between leading edges */
    float fadc_delta_t[DELTA_T_PULSER_AMP_CNT];
    float atwd_delta_t[DELTA_T_PULSER_AMP_CNT];

#ifdef DEBUG
    printf("Performing ATWD/FADC delta_t calibration...\r\n");
#endif
    
    /* Load the DOMApp FPGA if it's not already loaded */
    if (is_domapp_loaded() == 0) {
        if (load_domapp_fpga() != 0) {
            printf("Error loading domapp FPGA... aborting DAQ baseline calibration\r\n");
            return 1;
        }
    }

    /* Make sure the HV is OFF */
#if defined DOMCAL_REV2 || defined DOMCAL_REV3
    halDisablePMT_HV();
#else
    halDisableBaseHV();
    halPowerDownBase();
#endif
    halUSleep(250000);

	/* Set the sampling speed to 300 MHz to match conditions during data acquisition */
    short origSampDAC[2];
    origSampDAC[0] = halReadDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS);
    origSampDAC[1] = halReadDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS);
    short origBiasDAC = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
    short origDiscDAC = halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH);
    short origFADCRef = halReadDAC(DOM_HAL_DAC_FAST_ADC_REF);
	
	/* Check the fit r2 value before using frequency calibration results, if not good use a fixed DAC value*/
#ifdef DEBUG
	printf("Using a DAC value of %i for leading edge calibration of ATWD 0\n", (dom_calib->atwd0_freq_calib.r_squared>0.99 ? getFreqCalib(0, *dom_calib, 300.0) : ATWD_SAMPLING_SPEED_DAC));
#endif
	halWriteDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS, (dom_calib->atwd0_freq_calib.r_squared>0.99 ? getFreqCalib(0, *dom_calib, 300.0) : ATWD_SAMPLING_SPEED_DAC));
#ifdef DEBUG
	printf("Using a DAC value of %i for leading edge calibration of ATWD 1\n", (dom_calib->atwd1_freq_calib.r_squared>0.99 ? getFreqCalib(1, *dom_calib, 300.0) : ATWD_SAMPLING_SPEED_DAC));
#endif
	halWriteDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, (dom_calib->atwd1_freq_calib.r_squared>0.99 ? getFreqCalib(1, *dom_calib, 300.0) : ATWD_SAMPLING_SPEED_DAC));
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, DELTA_T_DISC_DAC);
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, DELTA_T_PEDESTAL_DAC);   
    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, DELTA_T_FADC_REF);       

    halUSleep(DAC_SET_WAIT);

    /*------------------------------------------------------------------*/
    /* Zero the running sums */
    int a;
    for (a = 0; a < 2; a++) {
        for(bin=0; bin<cnt; bin++) {
            ch0_baseline_wf[a][bin] = 0;
            atwd_avg[a][bin] = 0;
        }
    }
    for (bin=0; bin<fadc_cnt; bin++) 
        fadc_avg[bin] = 0;
    fadc_baseline = 0;

    /* Sampling frequencies */
    float freq[2];
	/* If good calibration was available we know we used 300 Hz, otherwise, try to figure out what frequency was used */
    freq[0] = 300.0;
    freq[1] = 300.0;
    float fadc_freq = FADC_CLOCK_FREQ;           

    /*------------------------------------------------------------------*/
    /* We do the DAQ loop multiple times                                
     *  - first record baselines (both ATWDs triggered)
     *  - record average waveforms for different pulser amplitudes
     */

    hal_FPGA_DOMAPP_disable_daq();

    int pulser_loop = 0;
    int pulser_idx = 0;
    int pulser_good_cnt_atwd = 0;
    int pulser_good_cnt_fadc = 0;
    while (pulser_idx < DELTA_T_PULSER_AMP_CNT) {

        /* Reset and zero the LBM */
        hal_FPGA_DOMAPP_lbm_reset();
        halUSleep(1000);
        hal_FPGA_DOMAPP_lbm_reset(); /* really! */
        memset(hal_FPGA_DOMAPP_lbm_address(), 0, WHOLE_LBM_MASK+1);
        unsigned lbmp = hal_FPGA_DOMAPP_lbm_pointer();

        if (pulser_loop == 0) {
            /* Set up forced trigger mode */           
            hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FORCED);
            hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
            hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_FORCED);
        }
        else {
            /* Now turn on the pulser and acquire average pulser waveforms     */
            /* Set the pulser rate to something reasonable */
            hal_FPGA_DOMAPP_cal_pulser_rate(DELTA_T_PULSER_RATE_HZ);

            /* Set the pulser amplitude */
            int pulser_dac = DELTA_T_PULSER_AMP_MIN + pulser_idx*DELTA_T_PULSER_AMP_STEP;
#ifdef DEBUG
            printf("Setting pulser dac to %d\r\n", pulser_dac);
#endif
            halWriteDAC(DOM_HAL_DAC_INTERNAL_PULSER, pulser_dac);

            /* Enable pulser calibration triggers */
            hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_SPE);
            hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FE_PULSER);
            hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_REPEAT);
        }

        hal_FPGA_DOMAPP_daq_mode(HAL_FPGA_DOMAPP_DAQ_MODE_ATWD_FADC);
        hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_TESTING);
        hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_WRAP);
        hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_OFF);
        hal_FPGA_DOMAPP_rate_monitor_enable(0);
        hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_OFF);
        
        int didMissedTrigWarning   = 0;
        npeds = n_atwd[0] = n_atwd[1] = 0;
        int numMissedTriggers = 0;
        
        /* Discard first couple of triggers */
        int numTrigs = DELTA_T_TRIG_CNT + 2;
        
        /* Enable BOTH ATWDs */
        hal_FPGA_DOMAPP_enable_atwds(HAL_FPGA_DOMAPP_ATWD_A|HAL_FPGA_DOMAPP_ATWD_B);
        hal_FPGA_DOMAPP_enable_daq(); 
        
        int trial,
            maxtrials = 10,
            trialtime = DELTA_T_WAIT_US,
            done      = 0;
        
        /* Get number of triggers requested */
        int trig; 
        for(trig=0; trig<numTrigs; trig++) {
            
            hal_FPGA_DOMAPP_cal_launch();
            done = 0;
            for(trial=0; !done && trial<maxtrials; trial++) {
                halUSleep(trialtime); /* Give FPGA time to write LBM without CPU hogging the bus */
                if(hal_FPGA_DOMAPP_lbm_pointer() >= lbmp+FPGA_DOMAPP_LBM_BLOCKSIZE*(trig+1)) done = 1;
            }
            
            if(!done) {
                numMissedTriggers++;
                if(!didMissedTrigWarning) {
                    didMissedTrigWarning++;
                    printf("WARNING: missed one or more calibration triggers for ATWD %d! "
                           "lbmp=0x%08x fpga_ptr=0x%08x", atwd, lbmp, hal_FPGA_DOMAPP_lbm_pointer());
                }
            }
        } /* Data-taking trigger loop */
        
        /*------------------------------------------------------------------*/
        /* Iterate back through triggers and extract baselines */
        
        /* Get bias DAC setting */
        short bias_dac = halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL);
        float bias_v = biasDAC2V(bias_dac);
        
        for(trig=0; trig<(numTrigs-numMissedTriggers); trig++) {
            
            /* Discard the first couple of triggers */
            if (trig >= 2) {
                
                /* Get lbm event */
                unsigned char * e = ((unsigned char *)hal_FPGA_DOMAPP_lbm_address()) + (lbmp & WHOLE_LBM_MASK);
                unsigned short * fadc_data = (unsigned short *) (e+0x10);
                unsigned short * atwd_data = (unsigned short *) (e+0x210);
                
                /* Count the waveforms into the running sums */
                ch = 0;
                float val;
                
                /* Figure out which ATWD we triggered from the header */
                /* We are ping-ponging!                               */
                a = e[10] & 0x1;

                for(bin=0; bin<cnt; bin++) {
                    val = (float)(atwd_data[cnt*ch + bin] & 0x3FF);
                    
                    /* Using ATWD calibration data, convert to V */
                    if (pulser_loop == 0) {
                        if (a == 0) {
                            ch0_baseline_wf[0][bin] += val * dom_calib->atwd0_gain_calib[ch][bin].slope
                                + dom_calib->atwd0_gain_calib[ch][bin].y_intercept - bias_v;
                        }
                        else {
                            ch0_baseline_wf[1][bin] += val * dom_calib->atwd1_gain_calib[ch][bin].slope
                                + dom_calib->atwd1_gain_calib[ch][bin].y_intercept - bias_v;
                        }
                    }
                    else {
                        if (a == 0) {
                            atwd_avg[0][bin] += val * dom_calib->atwd0_gain_calib[ch][bin].slope
                                + dom_calib->atwd0_gain_calib[ch][bin].y_intercept - bias_v
                                - ch0_baseline_wf[0][bin];
                        }
                        else {                        
                            atwd_avg[1][bin] += val * dom_calib->atwd1_gain_calib[ch][bin].slope
                                + dom_calib->atwd1_gain_calib[ch][bin].y_intercept - bias_v
                                - ch0_baseline_wf[1][bin];
                        }
                    }
                }

                for(bin=0; bin<fadc_cnt; bin++) {
                    if (pulser_loop == 0)
                        fadc_baseline += fadc_data[bin] & 0x3FF;
                    else
                        fadc_avg[bin] += (fadc_data[bin] & 0x3FF) - fadc_baseline;
                }
                n_atwd[a]++;
                npeds++;            

            } /* Triggers after the first couple */
            
            /* Go to next event in LBM */
            lbmp = lbmp + HAL_FPGA_DOMAPP_LBM_EVENT_SIZE;
            
        } /* Baseline waveform loop */
        
        /* Make sure we have enough waveforms */
        if (npeds < DELTA_T_MIN_WF) {
            printf("ERROR: Number of good waveforms fewer than minimum! ");
            printf("(%d good, %d required)\r\n", npeds, DELTA_T_MIN_WF);
            return 1;
        }
        
        /* Normalize the sums */
        for(a = 0; a < 2; a++) {
            for(bin=0; bin<cnt; bin++) {
                if (pulser_loop == 0)
                    ch0_baseline_wf[a][bin] /= (float)n_atwd[a] * dom_calib->amplifier_calib[0].value;
                else 
                    atwd_avg[a][bin] /= (float)n_atwd[a] * dom_calib->amplifier_calib[0].value;
            }
        }
        if (pulser_loop == 0) {
            /* FADC baseline is just a constant offset, we don't save whole waveform */
            fadc_baseline /= (float)npeds*fadc_cnt;
        }
        else {
            for(bin=0; bin<fadc_cnt; bin++) {
                fadc_avg[bin] /= (float)npeds;
                fadc_avg[bin] *= dom_calib->fadc_gain.value;
            }
        }

        if (pulser_loop != 0) {

            /*------------------------------------------------------------------*/
            /* Analyze average waveforms for time offset */
            
            /* Get sampling speed frequency in MHz for both ATWDs */    
            float le_x[128], le_y[128];        
            int le_cnt;
            
            float atwd_le[2], fadc_le;
            atwd_le[0] = atwd_le[1] = fadc_le = 0.0;
            int atwd_bad[2], fadc_bad;
            atwd_bad[0] = atwd_bad[1] = fadc_bad = 0;
            
			float pulse_fit_params[2];
			for (a = 0; a < 2; a++){
#ifdef DEBUG
				printf("Fitting ATWD %i\n",a);
#endif
				/* Estimate the pulse time and amplitude */
				guessPulseFitParams(atwd_avg[a],DELTA_T_START_BIN,cnt,freq[a],atwd_bin_time,
									(getTorroidType(dom_calib->fe_impedance) ? ATWD_NEW_PULSE_FIT_INTEGRAL_SCALE : ATWD_OLD_PULSE_FIT_INTEGRAL_SCALE),
									le_x,le_y,&le_cnt,pulse_fit_params);
				if(le_cnt>=2){ /* with only two points it will be a rather questionable fit, but it can technically be done */
					/* Do the fit */
					int fiterr = pulseFit(le_x,le_y,le_cnt,pulse_fit_params,(getTorroidType(dom_calib->fe_impedance)?atwd_new_pulse:atwd_old_pulse));
					if(fiterr != PULSE_FIT_NO_ERR){
#ifdef DEBUG
						printf("WARNING: ATWD %i pulse fit failed with error %i, discarding!\r\n", a, fiterr);
						printf("ATWD %i pulse fit: A=%f, x0=%f, leading edge time=%f\n",a,pulse_fit_params[0],pulse_fit_params[1],atwd_le[a]);
#endif
						atwd_bad[a] = 1;
					}
					else{
						atwd_le[a] = pulse_fit_params[1] + (getTorroidType(dom_calib->fe_impedance) ? ATWD_NEW_PULSE_LE_OFFSET : ATWD_OLD_PULSE_LE_OFFSET);
#ifdef DEBUG
						printf("ATWD %i pulse fit: A=%f, x0=%f, leading edge time=%f\n",a,pulse_fit_params[0],pulse_fit_params[1],atwd_le[a]);
#endif
					}
				}
				else{
#ifdef DEBUG
                    printf("WARNING: too few points to fit ATWD %i leading edge, discarding!\r\n", a);
#endif
                    atwd_bad[a] = 1;
                }
			}
            
#ifdef DEBUG
			printf("Fitting FADC\n");
#endif
			/* Estimate the pulse time and amplitude */
			guessPulseFitParams(fadc_avg,0,DELTA_T_STOP_SAMPLE,fadc_freq,fadc_bin_time,FADC_PULSE_FIT_INTEGRAL_SCALE,le_x,le_y,&le_cnt,pulse_fit_params);
			if(le_cnt>=2){ /* with only two points it will be a rather questionable fit, but it can technically be done */
				/* Do the fit */
				int fiterr = pulseFit(le_x,le_y,le_cnt,pulse_fit_params,fadc_pulse);
				if(fiterr != PULSE_FIT_NO_ERR){
#ifdef DEBUG
					printf("WARNING: FADC pulse fit failed with error %i, discarding!\r\n", fiterr);
					printf("FADC pulse fit: A=%f, x0=%f, leading edge time=%f\n",pulse_fit_params[0],pulse_fit_params[1],fadc_le);
#endif
					fadc_bad = 1;
				}
				else{
					fadc_le = pulse_fit_params[1] + FADC_PULSE_LE_OFFSET;
#ifdef DEBUG
					printf("FADC pulse fit: A=%f, x0=%f, leading edge time=%f\n",pulse_fit_params[0],pulse_fit_params[1],fadc_le);
#endif
				}
			}
			else{
#ifdef DEBUG
				printf("WARNING: too few points to fit FADC leading edge, discarding!\r\n");
#endif
				fadc_bad = 1;
			}
            
            /* Now compute offset assuming LE are in same position */
            if ((!atwd_bad[atwd]) && (!fadc_bad)) {
                fadc_delta_t[pulser_good_cnt_fadc] = atwd_le[atwd] - fadc_le;
#ifdef DEBUG
                printf("Offset between ATWD%d and FADC leading edges: %.1f ns\r\n", 
                       atwd, fadc_delta_t[pulser_good_cnt_fadc]);
#endif
                pulser_good_cnt_fadc++;
            }

            if ((!atwd_bad[0]) && (!atwd_bad[1])) {
                atwd_delta_t[pulser_good_cnt_atwd] = atwd_le[atwd] - atwd_le[atwd2];            
#ifdef DEBUG
                printf("Offset between ATWD%d and ATWD%d leading edges: %.1f ns\r\n", 
                       atwd, atwd2, atwd_delta_t[pulser_good_cnt_atwd]);
#endif   
                pulser_good_cnt_atwd++;
            }
        }

        /* Increment to pulser loop */
        if (pulser_loop == 0)
            pulser_loop++;
        else /* Increment pulser amplitude index */
            pulser_idx++;
        
    } /* End DAQ / pulser settings loop */

    /*------------------------------------------------------------------*/
    /* Calculate mean / error and save calibration results */
    
    /* Check that we had good results */
    if (pulser_good_cnt_fadc == 0) {
        dom_calib->fadc_delta_t.value = 0;
        dom_calib->fadc_delta_t.error = 0;
        printf("ERROR: no good FADC delta_t points -- all offsets set at zero!!!\r\n");
    }
    else {
        /* Calculate FADC delta_t average over all pulser settings */
        float fadc_delta_t_mean, fadc_delta_t_var;
        meanVarFloat(fadc_delta_t, pulser_good_cnt_fadc, &fadc_delta_t_mean, &fadc_delta_t_var);
        dom_calib->fadc_delta_t.value = fadc_delta_t_mean;
        dom_calib->fadc_delta_t.error = sqrt(fadc_delta_t_var / pulser_good_cnt_fadc);
    }

    if (pulser_good_cnt_atwd == 0) {
        dom_calib->atwd_delta_t[atwd].value = 0;
        dom_calib->atwd_delta_t[atwd].error = 0;
        dom_calib->atwd_delta_t[atwd2].value = 0;
        dom_calib->atwd_delta_t[atwd2].error = 0;
        printf("ERROR: no good ATWD delta_t points -- all offsets set at zero!!!\r\n");
    }
    else {
        /* Calculate ATWD2 delta_t average over all pulser settings */
        float atwd_delta_t_mean, atwd_delta_t_var;
        meanVarFloat(atwd_delta_t, pulser_good_cnt_atwd, &atwd_delta_t_mean, &atwd_delta_t_var);

        /* Sign of correction depends on whether we used same ATWD as transit time calibration */
        /* Goal is just to allow users to add transit time number with this delta, without */
        /* knowing which ATWD was used for what */
        if ((!dom_calib->transit_calib_valid) || (atwd == dom_calib->transit_calib_atwd)) {
            dom_calib->atwd_delta_t[atwd].value = 0;
            dom_calib->atwd_delta_t[atwd].error = 0;

            dom_calib->atwd_delta_t[atwd2].value = atwd_delta_t_mean;
            dom_calib->atwd_delta_t[atwd2].error = sqrt(atwd_delta_t_var / pulser_good_cnt_atwd);
        }
        else {
            dom_calib->atwd_delta_t[atwd2].value = 0;
            dom_calib->atwd_delta_t[atwd2].error = 0;

            dom_calib->atwd_delta_t[atwd].value = -atwd_delta_t_mean;
            dom_calib->atwd_delta_t[atwd].error = sqrt(atwd_delta_t_var / pulser_good_cnt_atwd);
        }

#ifdef DEBUG
        printf("Mean ATWD%d offset: %.2f +/- %.2f ns\r\n", atwd2, atwd_delta_t_mean, 
               sqrt(atwd_delta_t_var / pulser_good_cnt_atwd));
#endif
    }

    /*------------------------------------------------------------------*/
    /* Stop data taking */
    
    hal_FPGA_DOMAPP_disable_daq();
    hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_OFF);
    
    /* Return DACs to original setting */
    halWriteDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL, origBiasDAC);   
    halWriteDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH, origDiscDAC);
    halWriteDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS, origSampDAC[0]);
    halWriteDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS, origSampDAC[1]);
    halWriteDAC(DOM_HAL_DAC_FAST_ADC_REF, origFADCRef);

    return 0;

}
