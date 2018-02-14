#ifndef DOMCALCHECKER_H
#define DOMCALCHECKER_H

#include "DOMCalParser.h"

///Attempts to determin whether a given DOM ID likely corresponds to a DOM which
///actually has a PMT and an HV source installed.
bool probablyHasPMT(const std::string& domID);

struct DOMCalChecker{
	errorListing& errors;
	uint64_t currentMbID;
	
	DOMCalChecker(errorListing& el):errors(el),currentMbID(0){}
	
	void addError(const error& e){
		errors.addError(currentMbID,e);
	}
	
	static float minFitR2;
	static float ATWDAmplifierLow[3];
	static float ATWDAmplifierHigh[3];
	static float HVLow;
	static float HVHigh;
	static float transitTimeLow;
	static float transitTimeHigh;
	static float ATWDDeltaTLow;
	static float ATWDDeltaTHigh;
	static float ATWDDeltaTMaxError;
	static float FADCDeltaTLow;
	static float FADCDeltaTHigh;
	static float FADCDeltaTMaxError;
	static unsigned int minHVPoints;
	static unsigned int minTransitTimePoints;
	static unsigned int minPMTDiscPoints;
	
	///Check all of the calibration information in data
	///\param mbID the ID number of the DOM being checked; used for error reporting
	///\param data the data to check
	///\param domID the production ID of the DOM being checked; if non-empty this will be parsed
	///             in order to guess whether the DOM actually has a PMT and an HV source so that
	///             checking portions of the calibration which depend on that hardware can be suppressed
	void checkData(uint64_t mbID, const DOMCalRecord& data, std::string domID="");
	void checkATWDAmplifier(const DOMCalRecord& data, unsigned int amp);
	void checkHighVoltage(const DOMCalRecord& data);
	void checkTransitTime(const DOMCalRecord& data);
	void checkATWDDeltaT(const DOMCalRecord& data);
	void checkFADCDeltaT(const DOMCalRecord& data);
	void checkHVGainPoints(const DOMCalRecord& data);
	void checkPMTDiscPoints(const DOMCalRecord& data);
	void checkMainboardID(uint64_t mbID, const DOMCalRecord& data);
	
	template <typename FitType>
	void checkFitQuality(const FitType& fit, error error_template){
		if(fit.r_squared<minFitR2){
			error_template.message="Low r^2 value for "+error_template.message+" fit: "+lexical_cast<std::string>(fit.r_squared);
			addError(error_template);
		}
	}
};

#endif
