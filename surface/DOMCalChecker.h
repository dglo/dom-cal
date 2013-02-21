#ifndef DOMCALCHECKER_H
#define DOMCALCHECKER_H

#include "DOMCalParser.h"

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
	static float FADCDeltaTLow;
	static float FADCDeltaTHigh;
	static unsigned int minHVPoints;
	static unsigned int minTransitTimePoints;
	static unsigned int minPMTDiscPoints;
	
	void checkData(uint64_t mbID, const DOMCalRecord& data);
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
