#include "DOMCalChecker.h"
#include <cmath>
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

float DOMCalChecker::FADCDeltaTLow(-119.63);
float DOMCalChecker::FADCDeltaTHigh(-108.3);

unsigned int DOMCalChecker::minHVPoints(4);
unsigned int DOMCalChecker::minTransitTimePoints(4);
unsigned int DOMCalChecker::minPMTDiscPoints(4);

void DOMCalChecker::checkData(uint64_t mbID, const DOMCalRecord& data){
	currentMbID = mbID;
	
	if(!data.read.impedance)
		addError(error(WARNING,"Missing front-end impedance"));
	if(!data.read.date)
		addError(error(WARNING,"Missing date"));
	if(!data.read.time)
		addError(error(WARNING,"Missing time"));
	if(!data.read.temp)
		addError(error(WARNING,"Missing temperature"));
	
	if(data.read.domID)
		
	
	for(unsigned int i=0; i<16; i++)
		if(!data.read.DACValue[i])
			addError(error(ERROR,"Missing DAC setting "+boost::lexical_cast<std::string>(i)));
	for(unsigned int i=0; i<24; i++)
		if(!data.read.ADCValue[i])
			addError(error(ERROR,"Missing ADC setting "+boost::lexical_cast<std::string>(i)));
	
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
			addError(error(ERROR,"Missing ATWD "+boost::lexical_cast<std::string>(i)+" delta T"));
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
							   +boost::lexical_cast<std::string>(channel)+" bin "
							   +boost::lexical_cast<std::string>(bin)));
			else{
				checkFitQuality(data.atwd0_gain_calib[channel][bin],
								error(ERROR,"ATWD chip 0 channel"
									  +boost::lexical_cast<std::string>(channel)+" bin "
									  +boost::lexical_cast<std::string>(bin)+" response"));
			}
			if(!data.read.ATWD1GainCalib[channel][bin])
				addError(error(ERROR,"Missing ATWD response for chip 1, channel "
							   +boost::lexical_cast<std::string>(channel)+", bin "
							   +boost::lexical_cast<std::string>(bin)));
			else{
				checkFitQuality(data.atwd1_gain_calib[channel][bin],
								error(ERROR,"ATWD chip 1 channel"
									  +boost::lexical_cast<std::string>(channel)+" bin "
									  +boost::lexical_cast<std::string>(bin)+" response"));
			}
		}
	}
	
	for(unsigned int channel=0; channel<3; channel++){
		if(!data.read.ATWD0Baseline[channel])
			addError(error(ERROR,"Missing ATWD baseline for chip 0, channel "
						   +boost::lexical_cast<std::string>(channel)));
		if(!data.read.ATWD0Baseline[channel])
			addError(error(ERROR,"Missing ATWD baseline for chip 1, channel "
						   +boost::lexical_cast<std::string>(channel)));
	}
	
	for(unsigned int channel=0; channel<3; channel++){
		if(!data.read.AmplifierCalib[channel])
			addError(error(ERROR,"Missing amplifier calibration channel "
						   +boost::lexical_cast<std::string>(channel)));
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
							   +boost::lexical_cast<std::string>(channel)+", bin "
							   +boost::lexical_cast<std::string>(bin)));
			if(!data.read.ATWD1DAQBaseline[channel][bin])
				addError(error(ERROR,"Missing DAQ baseline for chip 1, channel "
							   +boost::lexical_cast<std::string>(channel)+", bin "
							   +boost::lexical_cast<std::string>(bin)));
		}
	}
	
	if(!data.read.HVBaselines)
		addError(error(ERROR,"Missing ATWD baselines as a function of high voltage"));
	
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

void DOMCalChecker::checkATWDAmplifier(const DOMCalRecord& data, unsigned int amp){
	if(data.amplifier_calib[amp].value<ATWDAmplifierLow[amp] ||
	   data.amplifier_calib[amp].value>ATWDAmplifierHigh[amp])
		addError(error(ERROR,"ATWD channel "+boost::lexical_cast<std::string>(amp)
					   +" amplifier gain out of range: "
					   +boost::lexical_cast<std::string>(data.amplifier_calib[amp].value)));
}

void DOMCalChecker::checkHighVoltage(const DOMCalRecord& data){
	float hv = pow(pow(10,7-data.hv_gain_calib.y_intercept),1./data.hv_gain_calib.slope);
	if(hv<HVLow || hv>HVHigh)
		addError(error(ERROR,"HV for gain of 10^7 out of range: "+boost::lexical_cast<std::string>(hv)));
}

void DOMCalChecker::checkTransitTime(const DOMCalRecord& data){
	if(data.transit_calib_points<minTransitTimePoints)
		addError(error(ERROR,"Too few valid transit time calibration points: "
					   +boost::lexical_cast<std::string>(data.transit_calib_points)));
	float hv = pow(pow(10,7-data.hv_gain_calib.y_intercept),1./data.hv_gain_calib.slope);
	float tt = (data.transit_calib.slope / sqrt(hv)) + data.transit_calib.y_intercept;
	if(tt<transitTimeLow || tt>transitTimeHigh)
		addError(error(ERROR,"Transit time for gain of 10^7 out of range: "+boost::lexical_cast<std::string>(tt)));
}

void DOMCalChecker::checkATWDDeltaT(const DOMCalRecord& data){
	//one ATWD is the time reference, and so its detla T should be exactly zero
	if(data.atwd_delta_t[0].value==0.0 && data.atwd_delta_t[0].error==0.0){
		//chip 0 is the reference
		if(data.atwd_delta_t[1].value<ATWDDeltaTLow || data.atwd_delta_t[1].value>ATWDDeltaTHigh)
			addError(error(ERROR,"ATWD 1 delta T out of range: "
						   +boost::lexical_cast<std::string>(data.atwd_delta_t[1].value)));
	}
	else if(data.atwd_delta_t[1].value==0.0 && data.atwd_delta_t[1].error==0.0){
		//chip 1 is the reference
		if(data.atwd_delta_t[0].value<ATWDDeltaTLow || data.atwd_delta_t[0].value>ATWDDeltaTHigh)
			addError(error(ERROR,"ATWD 1 delta T out of range: "
						   +boost::lexical_cast<std::string>(data.atwd_delta_t[0].value)));
	}
	else
		addError(error(ERROR,"Neither ATWD delta T is zero; cannot determine which is time reference ("
					   +boost::lexical_cast<std::string>(data.atwd_delta_t[0].value)+"+/-"
					   +boost::lexical_cast<std::string>(data.atwd_delta_t[0].error)+", "
					   +boost::lexical_cast<std::string>(data.atwd_delta_t[1].value)+"+/-"
					   +boost::lexical_cast<std::string>(data.atwd_delta_t[1].error)+")"));
}

void DOMCalChecker::checkFADCDeltaT(const DOMCalRecord& data){
	if(data.fadc_delta_t.value<FADCDeltaTLow || data.fadc_delta_t.value>FADCDeltaTHigh)
		addError(error(ERROR,"FADC delta T out of range: "
					   +boost::lexical_cast<std::string>(data.fadc_delta_t.value)));
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
					   +boost::lexical_cast<std::string>(validPoints)));
}

void DOMCalChecker::checkPMTDiscPoints(const DOMCalRecord& data){
	if(data.pmt_disc_calib_num_pts<minPMTDiscPoints)
		addError(error(ERROR,"Too few valid PMT discriminator points: "
					   +boost::lexical_cast<std::string>(data.pmt_disc_calib_num_pts)));
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