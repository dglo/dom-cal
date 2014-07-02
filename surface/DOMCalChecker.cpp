#include "DOMCalChecker.h"

#include <cmath>
#include <iomanip>
#include <set>

float DOMCalChecker::minFitR2(.99);

float DOMCalChecker::ATWDAmplifierLow[3] = {-19.8, -2.3, -0.28};
float DOMCalChecker::ATWDAmplifierHigh[3] = {-14.3, -1.5, -0.13};

float DOMCalChecker::HVLow(924);
float DOMCalChecker::HVHigh(1676);

float DOMCalChecker::transitTimeLow(130);
float DOMCalChecker::transitTimeHigh(156);


float DOMCalChecker::ATWDDeltaTLow(-8.0);
float DOMCalChecker::ATWDDeltaTHigh(8.0);
float DOMCalChecker::ATWDDeltaTMaxError(.5);

float DOMCalChecker::FADCDeltaTLow(-119.63);
float DOMCalChecker::FADCDeltaTHigh(-108.3);
float DOMCalChecker::FADCDeltaTMaxError(.5);

unsigned int DOMCalChecker::minHVPoints(4);
unsigned int DOMCalChecker::minTransitTimePoints(4);
unsigned int DOMCalChecker::minPMTDiscPoints(4);

void DOMCalChecker::checkData(uint64_t mbID, const DOMCalRecord& data, std::string domID){
	currentMbID = mbID;
	
	if(!data.read.exists){
		addError(error(ERROR,"No actual data found"));
		if(data.runningTime!=0)
			addError(error(ERROR,"Calibration did not complete"));
		return;
	}
	else if(data.runningTime!=0){ //there was a .running file
		if(data.calTime==0)
			addError(error(ERROR,"Calibration did not complete"));
		else{
			double timeDiff=difftime(data.calTime,data.runningTime);
			if(timeDiff>0.0)
				addError(error(WARNING,"Old .running file"));
			else
				addError(error(ERROR,"Calibration did not complete"));
		}
	}
	
	if(!data.read.impedance)
		addError(error(WARNING,"Missing front-end impedance"));
	if(!data.read.date)
		addError(error(WARNING,"Missing date"));
	if(!data.read.time)
		addError(error(WARNING,"Missing time"));
	if(!data.read.temp)
		addError(error(WARNING,"Missing temperature"));
	
	if(data.read.domID){}
		
	
	for(unsigned int i=0; i<16; i++)
		if(!data.read.DACValue[i])
			addError(error(ERROR,"Missing DAC setting "+lexical_cast<std::string>(i)));
	for(unsigned int i=0; i<24; i++)
		if(!data.read.ADCValue[i])
			addError(error(ERROR,"Missing ADC setting "+lexical_cast<std::string>(i)));
	
	if(!data.read.FADCBaseline)
		addError(error(ERROR,"Missing FADC baseline"));
	else{
		checkFitQuality(data.fadc_baseline,error(ERROR,"FADC Baseline"));
	}
	
	if(!data.read.FADCGain)
		addError(error(ERROR,"Missing FADC gain"));
	
	if(!data.read.FADCDeltaT)
		addError(error(ERROR,"Missing FADC delta T"));
	else
		checkFADCDeltaT(data);
	
	for(unsigned int i=0; i<2; i++){
		if(!data.read.ATWDDeltaT[i])
			addError(error(ERROR,"Missing ATWD "+lexical_cast<std::string>(i)+" delta T"));
	}
	if(data.read.ATWDDeltaT[0] && data.read.ATWDDeltaT[1])
		checkATWDDeltaT(data);
	
	if(!data.read.SPEDiscCalib)
		addError(error(ERROR,"Missing SPE discriminator calibration"));
	else{
		checkFitQuality(data.spe_disc_calib,error(ERROR,"SPE discriminator calibration"));
	}
	if(!data.read.MPEDiscCalib)
		addError(error(ERROR,"Missing MPE discriminator calibration"));
	else{
		checkFitQuality(data.mpe_disc_calib,error(ERROR,"MPE discriminator calibration"));
	}
	
	for(unsigned int channel=0; channel<3; channel++){
		for(unsigned int bin=0; bin<128; bin++){
			if(!data.read.ATWD0GainCalib[channel][bin])
				addError(error(ERROR,"Missing ATWD response for chip 0 channel "
							   +lexical_cast<std::string>(channel)+" bin "
							   +lexical_cast<std::string>(bin)));
			else{
				checkFitQuality(data.atwd0_gain_calib[channel][bin],
								error(ERROR,"ATWD chip 0 channel"
									  +lexical_cast<std::string>(channel)+" bin "
									  +lexical_cast<std::string>(bin)+" response"));
			}
			if(!data.read.ATWD1GainCalib[channel][bin])
				addError(error(ERROR,"Missing ATWD response for chip 1, channel "
							   +lexical_cast<std::string>(channel)+", bin "
							   +lexical_cast<std::string>(bin)));
			else{
				checkFitQuality(data.atwd1_gain_calib[channel][bin],
								error(ERROR,"ATWD chip 1 channel"
									  +lexical_cast<std::string>(channel)+" bin "
									  +lexical_cast<std::string>(bin)+" response"));
			}
		}
	}
	
	for(unsigned int channel=0; channel<3; channel++){
		if(!data.read.ATWD0Baseline[channel])
			addError(error(ERROR,"Missing ATWD baseline for chip 0, channel "
						   +lexical_cast<std::string>(channel)));
		if(!data.read.ATWD0Baseline[channel])
			addError(error(ERROR,"Missing ATWD baseline for chip 1, channel "
						   +lexical_cast<std::string>(channel)));
	}
	
	for(unsigned int channel=0; channel<3; channel++){
		if(!data.read.AmplifierCalib[channel])
			addError(error(ERROR,"Missing amplifier calibration channel "
						   +lexical_cast<std::string>(channel)));
		else
			checkATWDAmplifier(data,channel);
	}
	
	if(!data.read.ATWD0FreqCalib)
		addError(error(ERROR,"Missing ATWD 0 frequency calibration"));
	else{
		checkFitQuality(data.atwd0_freq_calib,error(ERROR,"ATWD 0 frequency"));
	}
	if(!data.read.ATWD1FreqCalib)
		addError(error(ERROR,"Missing ATWD 1 frequency calibration"));
	else{
		checkFitQuality(data.atwd1_freq_calib,error(ERROR,"ATWD 1 frequency"));
	}
	
	for(unsigned int channel=0; channel<3; channel++){
		for(unsigned int bin=0; bin<128; bin++){
			if(!data.read.ATWD0DAQBaseline[channel][bin])
				addError(error(ERROR,"Missing DAQ baseline for chip 0, channel "
							   +lexical_cast<std::string>(channel)+", bin "
							   +lexical_cast<std::string>(bin)));
			if(!data.read.ATWD1DAQBaseline[channel][bin])
				addError(error(ERROR,"Missing DAQ baseline for chip 1, channel "
							   +lexical_cast<std::string>(channel)+", bin "
							   +lexical_cast<std::string>(bin)));
		}
	}
	
	if(!data.read.HVBaselines)
		addError(error(ERROR,"Missing ATWD baselines as a function of high voltage"));
	
	//Guess whether to check HV related quantities
	if(domID.empty() || probablyHasPMT(domID)){
		if(!data.read.HVGainCalib)
			addError(error(ERROR,"Missing high voltage/gain calibration"));
		else{
			checkFitQuality(data.hv_gain_calib,error(ERROR,"HV Gain"));
			checkHighVoltage(data);
		}
		
		if(!data.read.TransitTimeCalib)
			addError(error(ERROR,"Missing transit time calibration"));
		else{
			checkFitQuality(data.transit_calib,error(ERROR,"transit time"));
			if(data.read.HVGainCalib) //need to have an HV Gain calib to do this check
				checkTransitTime(data);
		}
		
		if(!data.read.HVHistograms)
			addError(error(ERROR,"Missing charge histograms"));
		else
			checkHVGainPoints(data);
		
		if(!data.read.PMTDiscCalib)
			addError(error(ERROR,"Missing PMT discriminator calibration"));
		else{
			checkFitQuality(data.pmt_disc_calib,error(ERROR,"PMT discriminator"));
			checkPMTDiscPoints(data);
		}
	}
}

void DOMCalChecker::checkATWDAmplifier(const DOMCalRecord& data, unsigned int amp){
	if(data.amplifier_calib[amp].value<ATWDAmplifierLow[amp] ||
	   data.amplifier_calib[amp].value>ATWDAmplifierHigh[amp])
		addError(error(ERROR,"ATWD channel "+lexical_cast<std::string>(amp)
					   +" amplifier gain out of range: "
					   +lexical_cast<std::string>(data.amplifier_calib[amp].value)));
}

void DOMCalChecker::checkHighVoltage(const DOMCalRecord& data){
	float hv = pow(pow(10,7-data.hv_gain_calib.y_intercept),1./data.hv_gain_calib.slope);
	if(hv<HVLow || hv>HVHigh)
		addError(error(ERROR,"HV for gain of 10^7 out of range: "+lexical_cast<std::string>(hv)));
}

void DOMCalChecker::checkTransitTime(const DOMCalRecord& data){
	if(data.transit_calib_points<minTransitTimePoints)
		addError(error(ERROR,"Too few valid transit time calibration points: "
					   +lexical_cast<std::string>(data.transit_calib_points)));
	float hv = pow(pow(10,7-data.hv_gain_calib.y_intercept),1./data.hv_gain_calib.slope);
	float tt = (data.transit_calib.slope / sqrt(hv)) + data.transit_calib.y_intercept;
	if(tt<transitTimeLow || tt>transitTimeHigh)
		addError(error(ERROR,"Transit time for gain of 10^7 out of range: "+lexical_cast<std::string>(tt)));
}

void DOMCalChecker::checkATWDDeltaT(const DOMCalRecord& data){
	//one ATWD is the time reference, and so its detla T should be exactly zero
	if(data.atwd_delta_t[0].value==0.0 && data.atwd_delta_t[0].error==0.0){
		//chip 0 is the reference
		if(data.atwd_delta_t[1].value<ATWDDeltaTLow || data.atwd_delta_t[1].value>ATWDDeltaTHigh)
			addError(error(ERROR,"ATWD 1 delta T out of range: "
						   +lexical_cast<std::string>(data.atwd_delta_t[1].value)));
		if(data.atwd_delta_t[1].error>ATWDDeltaTMaxError)
			addError(error(WARNING,"Abnormally large error on ATWD 1 delta T value: "
						   +lexical_cast<std::string>(data.atwd_delta_t[1].error)));
	}
	else if(data.atwd_delta_t[1].value==0.0 && data.atwd_delta_t[1].error==0.0){
		//chip 1 is the reference
		if(data.atwd_delta_t[0].value<ATWDDeltaTLow || data.atwd_delta_t[0].value>ATWDDeltaTHigh)
			addError(error(ERROR,"ATWD 1 delta T out of range: "
						   +lexical_cast<std::string>(data.atwd_delta_t[0].value)));
		if(data.atwd_delta_t[0].error>ATWDDeltaTMaxError)
			addError(error(WARNING,"Abnormally large error on ATWD 0 delta T value: "
						   +lexical_cast<std::string>(data.atwd_delta_t[0].error)));
	}
	else
		addError(error(ERROR,"Neither ATWD delta T is zero; cannot determine which is time reference ("
					   +lexical_cast<std::string>(data.atwd_delta_t[0].value)+"+/-"
					   +lexical_cast<std::string>(data.atwd_delta_t[0].error)+", "
					   +lexical_cast<std::string>(data.atwd_delta_t[1].value)+"+/-"
					   +lexical_cast<std::string>(data.atwd_delta_t[1].error)+")"));
}

void DOMCalChecker::checkFADCDeltaT(const DOMCalRecord& data){
	if(data.fadc_delta_t.value<FADCDeltaTLow || data.fadc_delta_t.value>FADCDeltaTHigh)
		addError(error(ERROR,"FADC delta T out of range: "
					   +lexical_cast<std::string>(data.fadc_delta_t.value)));
	if(data.fadc_delta_t.error>FADCDeltaTMaxError)
		addError(error(WARNING,"Abnormally large error on FADC delta T value: "
					   +lexical_cast<std::string>(data.fadc_delta_t.error)));
}

void DOMCalChecker::checkHVGainPoints(const DOMCalRecord& data){
	unsigned int validPoints=0;
	//only points with distinct voltages are useful; 
	//if an IceTop DOM has 4 valid points at the exact same voltage the fit will be useless
	std::set<unsigned int> voltages;
	for(unsigned int i=0; i<data.num_histos; i++){
		if(data.histogram_data[i].is_filled && data.histogram_data[i].convergent){
			if(voltages.count(data.histogram_data[i].voltage))
				continue; //this voltage already has a valid point
			validPoints++;
			voltages.insert(data.histogram_data[i].voltage);
		}
	}
	
	if(validPoints<minHVPoints)
		addError(error(ERROR,"Too few distinct, valid HV Gain calibration points: "
					   +lexical_cast<std::string>(validPoints)));
}

void DOMCalChecker::checkPMTDiscPoints(const DOMCalRecord& data){
	if(data.pmt_disc_calib_num_pts<minPMTDiscPoints)
		addError(error(ERROR,"Too few valid PMT discriminator points: "
					   +lexical_cast<std::string>(data.pmt_disc_calib_num_pts)));
}

void DOMCalChecker::checkMainboardID(uint64_t mbID, const DOMCalRecord& data){
	uint64_t dataID;
	std::istringstream ss(data.dom_id);
	ss >> std::hex >> dataID;
	if(ss.fail())
		addError(error(WARNING,"Malformed mainboard id: "
					   +std::string(data.dom_id)));
	else if(dataID!=mbID)
		addError(error(WARNING,"Mainboard ID does not match: "
					   +std::string(data.dom_id)));
}

bool probablyHasPMT(const std::string& domID){
	//DOMs with "Syn", "Sync", "Ref", or "Trig" are just mainboards.
	if(domID.find("Syn")!=std::string::npos)
		return(false);
	if(domID.find("Ref")!=std::string::npos)
		return(false);
	if(domID.find("Trig")!=std::string::npos)
		return(false);
	//otherwise guess that it is a complete DOM
	return(true);
}