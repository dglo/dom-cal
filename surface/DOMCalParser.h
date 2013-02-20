#ifndef DOMCALPARSER_H
#define DOMCALPARSER_H

#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <ctime>
#include <stdint.h>

#include <dirent.h>

#include "domcal.h"
#include "XMLUtils.h"

//ugly, but functional subclass of calib_data which can track whether each data item actually has 
//a known value
struct DOMCalRecord : public calib_data{
public:
	struct readFlags{
		bool exists;
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
	
	time_t calTime, runningTime;
	
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
		calTime=0;
		runningTime=0;
	}
	
	DOMCalRecord(const DOMCalRecord& rec):
	calib_data(rec),read(rec.read),calTime(rec.calTime),runningTime(rec.runningTime){
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

struct mainboardID{
	uint64_t id;
	mainboardID(uint64_t i):id(i){}
};

std::ostream& operator<<(std::ostream&, const mainboardID&);

// Simple means of iterating over the filesystem
// All default constructed instances are considered equivalent 'end' iterators
struct directory_iterator{
public:
	//a structure for holding filesystem paths which makes inspecting them slightly more convenient
	struct path_t{
	private:
		std::string dirpath;
		std::string itemname;
	public:
		path_t(const std::string& d, const std::string& n):dirpath(d),itemname(n){}
		
		std::string string() const{
			return(dirpath+"/"+itemname);
		}
		std::string name() const{
			return(itemname);
		}
		//extracts the name of the item referred to by this path, without the extension, if any
		//So, the stem of /A/B/file.ext is "file"
		std::string stem() const{
			size_t dotIdx=itemname.rfind('.');
			if(dotIdx==std::string::npos)
				return(itemname);
			return(itemname.substr(0,dotIdx));
		}
		//extracts the extension of the item referred to by this path, if any, omitting the dot
		//So, the extension of /A/B/file.ext is "ext"
		std::string extension() const{
			size_t dotIdx=itemname.rfind('.');
			if(dotIdx==std::string::npos || dotIdx==itemname.size()-1)
				return("");
			return(itemname.substr(dotIdx+1));
		}
	};
	
	//represents an item found in a directory
	struct directory_entry{
	private:
		const std::string dirPath;
		dirent* item;
		
		friend class directory_iterator;
		friend bool operator==(const directory_iterator& d1, const directory_iterator& d2);
		friend bool is_directory(const directory_iterator::directory_entry& entry);
	public:
		directory_entry(const std::string& dp, dirent* i):dirPath(dp),item(i){}
		
		path_t path() const{
			return(path_t(dirPath,item->d_name));
		}
	};
private:
	std::string path;
	DIR* dirp;
	directory_entry curItem;
	
	friend bool operator==(const directory_iterator& d1, const directory_iterator& d2);
public:
	directory_iterator():dirp(NULL),curItem(path,NULL){}
	
	directory_iterator(const std::string& path):
	path(path),dirp(opendir(path.c_str())),
	curItem(this->path,dirp?readdir(dirp):NULL)
	{}
	
	~directory_iterator(){
		if(dirp)
			closedir(dirp);
	}
	const directory_entry& operator*() const{
		return(curItem);
    }
	const directory_entry* operator->() const{
        return(&curItem);
    }
	const directory_entry& operator++(){ //prefix increment
		if(dirp)
			curItem.item=readdir(dirp);
		return(curItem);
	}
	directory_entry operator++ (int){ //postfix increment
		directory_entry temp(curItem);
		if(dirp)
			curItem.item=readdir(dirp);
		return(temp);
	}
};

bool operator==(const directory_iterator& d1, const directory_iterator& d2);
bool operator!=(const directory_iterator& d1, const directory_iterator& d2);
bool is_directory(const directory_iterator::directory_entry& entry);
// end of filesystem infrastructure

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
	
	bool domHasFatalError(uint64_t mbID) const{
		std::map<uint64_t,std::vector<error> >::const_iterator dom=errors.find(mbID);
		if(dom==errors.end()) //no recorded errors definitely means no no fatal errors
			return(false);
		for(std::vector<error>::const_iterator error=dom->second.begin(), end=dom->second.end(); error!=end; error++){
			if(error->severity==FATAL_ERROR)
				return(true);
		}
		return(false);
	}
};

value_error readValueWithError(xmlNode* xvalue);
linear_fit readLinearFit(xmlNode* xfit);
quadratic_fit readQuadraticFit(xmlNode* xfit);

struct DOMCalParser{
	errorListing& errors;
	bool fatalErrorOccurred;
	uint64_t currentMbID;
	
	DOMCalParser(errorListing& el):errors(el),currentMbID(0){}
	
	void addError(const error& e){
		errors.addError(currentMbID,e);
		if(e.severity==FATAL_ERROR)
			fatalErrorOccurred=true;
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
	
	DOMCalRecord parseDOMCalFile(const std::string& file, uint64_t mbID);
};

//given a filename of the form .*domcal_[0-9A-Fa-f]{12}.* extracts the value of the 
//mainboard id substring (the [0-9A-Fa-f]{12}) into mbID and return true, otherwise return false
bool extractMainboardID(const std::string& filename, uint64_t& mbID);

void parseAllDOMCalFiles(const std::string& directory, std::map<uint64_t,DOMCalRecord>& data, errorListing& errors);

#endif