#ifndef DOMCALPARSER_H
#define DOMCALPARSER_H

#include <string>
#include <map>
#include <vector>
#include <cstring>

#include <boost/filesystem.hpp>

#include <domcal.h>
#include "libXMLUtils.h"

//ugly, but functional subclass of calib_data which can track whether each data item actually has 
//a known value
struct DOMCalRecord : public calib_data{
public:
	struct readFlags{
		bool impedance;
		bool date;
		bool time;
		bool domID;
		bool temp;
		bool DACValue[16];
		bool ADCValue[24];
		bool FADCBaseline;
		bool FADCGain;
		bool FADCDeltaT;
		bool ATWDDeltaT[2];
		bool SPEDiscCalib;
		bool MPEDiscCalib;
		bool ATWD0GainCalib[3][128];
		bool ATWD1GainCalib[3][128];
		bool ATWD0Baseline[3];
		bool ATWD1Baseline[3];
		bool AmplifierCalib[3];
		bool ATWD0FreqCalib;
		bool ATWD1FreqCalib;
		bool ATWD0DAQBaseline[3][128];
		bool ATWD1DAQBaseline[3][128];
		bool HVBaselines;
		bool HVGainCalib;
		bool TransitTimeCalib;
		bool HVHistograms;
		bool PMTDiscCalib;
	} read;
	
	DOMCalRecord(){
		//Zero initialize everything
		//This is mostly to set all of the boolean flags in this class to false
		//but also puts the base object in a completely known state
		memset(this,0,sizeof(DOMCalRecord));
		//A minor detail is that this sets the pointer members of the base object to zero, 
		//which isn't the right thing to do on systems where NULL!=0. So patch this up here, 
		//in case someone sadistic compiles this on such a system. 
		baseline_data=NULL;
		histogram_data=NULL;
	}
	
	DOMCalRecord(const DOMCalRecord& rec):
	calib_data(rec),read(rec.read){
		if(rec.baseline_data){
			baseline_data=new hv_baselines[rec.num_baselines];
			memcpy(baseline_data,rec.baseline_data,num_baselines*sizeof(hv_baselines));
		}
		else
			baseline_data=NULL;
		if(rec.histogram_data){
			histogram_data=new hv_histogram[rec.num_histos];
			memcpy(histogram_data,rec.histogram_data,num_histos*sizeof(hv_histogram));
			for(unsigned int i=0; i<num_histos; i++){
				if(histogram_data[i].x_data!=NULL){
					float* new_data=new float[histogram_data[i].bin_count];
					memcpy(new_data,histogram_data[i].x_data,histogram_data[i].bin_count*sizeof(float));
					histogram_data[i].x_data=new_data;
				}
				if(histogram_data[i].y_data!=NULL){
					float* new_data=new float[histogram_data[i].bin_count];
					memcpy(new_data,histogram_data[i].y_data,histogram_data[i].bin_count*sizeof(float));
					histogram_data[i].y_data=new_data;
				}
				if(histogram_data[i].fit!=NULL){
					float* new_data=new float[5];
					memcpy(new_data,histogram_data[i].fit,5*sizeof(float));
					histogram_data[i].fit=new_data;
				}
			}
		}
		else
			histogram_data=NULL;
	}
	
	~DOMCalRecord(){
		delete[] baseline_data;
		for(unsigned int i=0; i<num_histos; i++){
			delete[] histogram_data[i].x_data;
			delete[] histogram_data[i].y_data;
			delete[] histogram_data[i].fit;
		}
		delete[] histogram_data;
	}
private:
	DOMCalRecord& operator=(const DOMCalRecord& rec);
};

enum severity_t {WARNING, ERROR, FATAL_ERROR};

const char* severityString(severity_t s);

struct error{
	severity_t severity;
	unsigned long lineNumber;
	std::string message;
	
	error(severity_t s):
	severity(s),lineNumber(0){}
	error(severity_t s, unsigned long n):
	severity(s),lineNumber(n){}
	error(severity_t s, const std::string& m):
	severity(s),lineNumber(0),message(m){}
	error(severity_t s, unsigned long n, const std::string& m):
	severity(s),lineNumber(n),message(m){}
};

std::ostream& operator<<(std::ostream& os, const error& e);

struct errorListing{
public:
	std::map<uint64_t,std::vector<error> > errors;
	
	void addError(uint64_t mbID, const error& e){
		errors[mbID].push_back(e);
	}
};

value_error readValueWithError(xmlNode* xvalue);
linear_fit readLinearFit(xmlNode* xfit);
quadratic_fit readQuadraticFit(xmlNode* xfit);

struct DOMCalParser{
	errorListing& errors;
	uint64_t currentMbID;
	
	DOMCalParser(errorListing& el):errors(el),currentMbID(0){}
	
	void addError(const error& e){
		errors.addError(currentMbID,e);
	}
	
	template <typename T>
	T getAttributeWithErrorContext(xmlNode* node, const std::string& name, error error_template){
		try{
			return(getAttribute<T>(node,name));
		} catch(xmlParseError& err){
			error_template.message+=err.what();
			error_template.lineNumber=xmlGetLineNo(node);
			addError(error_template);
			throw;
		}
	}
	
	template <typename T>
	T callWithErrorContext(T (*func)(xmlNode*), xmlNode* node, error error_template){
		try{
			return(func(node));
		} catch(xmlParseError& err){
			error_template.message+=err.what();
			error_template.lineNumber=xmlGetLineNo(node);
			addError(error_template);
			throw;
		}
	}
	
	void parseATWDBinGain(DOMCalRecord& data, xmlNode* node);
	void parseDACSetting(DOMCalRecord& data, xmlNode* node);
	void parseADCSetting(DOMCalRecord& data, xmlNode* node);
	void parseChannelBaseline(xmlNode* node, unsigned int& chip_id, unsigned int& channel, float& baseline);
	void parseBaseline(DOMCalRecord& data, xmlNode* node);
	void parseHVHistogram(DOMCalRecord& data, xmlNode* node);
	void parseAmplifier(DOMCalRecord& data, xmlNode* node);
	void parseATWDDeltaT(DOMCalRecord& data, xmlNode* node);
	void parseATWDFrequency(DOMCalRecord& data, xmlNode* node);
	void parseDiscriminator(DOMCalRecord& data, xmlNode* node);
	void parseTemperature(DOMCalRecord& data, xmlNode* node);
	void parseTransitTime(DOMCalRecord& data, xmlNode* node);
	void parseHVGainCal(DOMCalRecord& data, xmlNode* node);
	void parseImpedance(DOMCalRecord& data, xmlNode* node);
	void parseFADCGain(DOMCalRecord& data, xmlNode* node);
	void parseFADCBaseline(DOMCalRecord& data, xmlNode* node);
	void parseFADCDeltaT(DOMCalRecord& data, xmlNode* node);
	void parseDAQBaselineBin(xmlNode* node, unsigned int& chip_id, unsigned int& channel, unsigned int& bin, float& sample);
	void parseDAQBaseline(DOMCalRecord& data, xmlNode* node);
	void parsePMTDiscCal(DOMCalRecord& data, xmlNode* node);
	void parseMainboardID(DOMCalRecord& data, xmlNode* node);
	void parseDate(DOMCalRecord& data, xmlNode* node);
	void parseTime(DOMCalRecord& data, xmlNode* node);
	
	DOMCalRecord parseDOMCalFile(const boost::filesystem::path& file, uint64_t mbID);
};

void parseAllDOMCalFiles(const boost::filesystem::path& directory, std::map<int64_t,DOMCalRecord>& data, errorListing& errors);

#endif