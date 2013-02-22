#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <sstream>
#include <stdint.h>
#include <cmath>

#include "DOMListing.h"
#include "DOMCalParser.h"
#include "DOMCalChecker.h"

//never recommend voltage beyond this range
const float minHVLimit=900;
const float maxHVLimit=1850;

//try to use a range of voltages at least this large
const float minHVRange=400;

//try to choose voltages surrounding these gain values
const double iniceLogGainTarget=7.0;
const double icetopLogGainTarget=log10(5e6);

const int defaultCalATWD = -1;

struct overrideSettings{
	bool minHVSet;
	float minHV;
	bool maxHVSet;
	float maxHV;
	bool atwdSet;
	int atwd;
	
	overrideSettings():
	minHVSet(false),maxHVSet(false),atwdSet(false){}
	
	bool full() const{
		return(minHVSet && maxHVSet && atwdSet);
	}
};

std::istream& operator>>(std::istream& is, overrideSettings& s){
	std::string item;
	std::istringstream ss;
	
	//minHV
	is >> item;
	if(is.fail())
		return(is);
	if(item!="-"){
		ss.clear();
		ss.str(item);
		ss >> s.minHV;
		if(ss.fail()){
			is.setstate(is.rdstate() | std::ios::failbit);
			return(is);
		}
		s.minHVSet=true;
	}
	//maxHV
	is >> item;
	if(is.fail())
		return(is);
	if(item!="-"){
		ss.clear();
		ss.str(item);
		ss >> s.maxHV;
		if(ss.fail()){
			is.setstate(is.rdstate() | std::ios::failbit);
			return(is);
		}
		s.maxHVSet=true;
	}
	//calATWD
	is >> item;
	if(is.fail())
		return(is);
	if(item!="-"){
		ss.clear();
		ss.str(item);
		ss >> s.atwd;
		if(ss.fail()){
			is.setstate(is.rdstate() | std::ios::failbit);
			return(is);
		}
		s.atwdSet=true;
	}
	
	return(is);
}

bool parseOverrideFile(const std::string& path, std::map<uint64_t,overrideSettings>& overrides){
	std::ifstream file(path.c_str());
	if(!file.good()){
		std::cerr << "Unable to open " << path << " for reading" << std::endl;
		return(false);
	}
	unsigned int lineNum=1;
	while(true){
		uint64_t mbID;
		overrideSettings s;
		file >> std::hex >> mbID >> std::dec >> s;
		if(file.eof())
			break;
		if(file.fail()){
			std::cerr << "failure parsing at line " << lineNum << std::endl;
			return(false);
		}
		overrides.insert(std::make_pair(mbID,s));
	}
	
	return(true);
}

//guess whether a DOM is part of IceTop based on its OMKey
bool probablyIceTop(const std::string& omkey){
	int string=0;
	unsigned int om=0;
	char dummy;
	std::istringstream ss(omkey);
	ss >> string >> dummy;
	if(ss.fail() || ss.eof() || dummy!='-')
		return(false); //if we can't read it, assume in-ice
	ss >> om;
	if(ss.fail())
		return(false); //if we can't read it, assume in-ice
	return(om>60 && om<=64);
}

//limit hv to be within the recommended 'safe' range
float limitHV(float hv){
	if(hv<minHVLimit)
		hv=minHVLimit;
	else if(hv>maxHVLimit)
		hv=maxHVLimit;
	return(hv);
}

//try to make sure that the range [minHV,maxHV] is not smaller than minHVRange, without
//going outside of the recommneded 'safe' range or ignoring a user-specified override
void expandHVRange(float& minHV, float& maxHV, const overrideSettings& override){
	if((maxHV-minHV)<minHVRange){
		//if one end of the range was specified as an override or is already at the limit
		//we shouldn't try to move that one
		bool limitedBelow=(override.minHVSet || minHV<=minHVLimit);
		bool limitedAbove=(override.maxHVSet || maxHV<=maxHVLimit);
		
		if(!limitedBelow && limitedAbove) //need to try to move the lower limit
			minHV=limitHV(maxHV-minHVRange);
		else if(limitedBelow && !limitedAbove) //need to try to move the upper limit
			maxHV=limitHV(minHV+minHVRange);
		else if(!limitedBelow && !limitedAbove){ //free to move both limits
			float middle=(minHV+maxHV)/2.;
			minHV=limitHV(middle-minHVRange/2);
			maxHV=limitHV(middle-minHVRange/2);
			//it could be the case that we now hit one of the limits, so the range is still to small
			//as a lazy way to address this, just invoke this function again
			//the recursion won't get out of control because if this happens at least one limit is now fixed
			expandHVRange(minHV,maxHV,override);
		}
		//else <=> limitedBelow && limitedAbove <=> can't make any changes
	}
	//else nothing to do
}

void usage(){
	std::cerr << "Usage: DOMCalConfig \n"
	"   -L [DOM listing file] (providing mappings from mainboard IDs to DOM IDs)\n"
	"   --override [override file] Specify a path to a file which contains configuration\n"
	"     settings to be used instead of any which might be generated from existing\n"
	"     calibration data.\n"
	"     The file must contain whitespace separated data with the following columns:\n"
	"      Mainboard ID, Minimum HV, Maximum HV, Preferred ATWD"
	"     any of the latter fields may be '-', in which case that configuration\n"
	"     parameter is not overridden. "
	"   --assumeInIce Treat all DOMs as if they are in-ice"
	<< std::endl;
}

int main(int argc, char* argv[]){
	std::map<uint64_t,domIdentifier> knownDOMs; //the names and ID numbers of DOMs
	std::map<uint64_t,DOMCalRecord> data;
	std::map<uint64_t,overrideSettings> overrides;
	errorListing errors;
	bool assumeInIce=false;
	
	if(argc<=1){
		usage();
		return(1);
	}
	
	for(unsigned int i=1; i<argc; i++){
		std::string arg=argv[i];
		if(arg=="--override"){
			if(i+1==argc){
				std::cerr << "missing override file path after --override" << std::endl;
				return(1);
			}
			i++;
			if(!parseOverrideFile(argv[i],overrides)){
				std::cerr << "Failed to parse override file " << argv[i] << std::endl;
				return(1);
			}
		}
		else if(arg=="--assumeInIce")
			assumeInIce=true;
		else if(arg=="-L"){
			if(i==argc-1){
				std::cerr << "Missing DOM listing file path after -L flag" << std::endl;
				return(1);
			}
			i++;
			std::ifstream listingFile(argv[i]);
			if(!listingFile.good()){
				std::cerr << "Unable to open DOM listing file " << argv[i] << std::endl;
				return(1);
			}
			loadDOMListing(listingFile,knownDOMs);
			listingFile.close();
		}
		else if(arg=="-h" || arg=="-help" || arg=="--help")
			usage();
		else
			parseAllDOMCalFiles(arg,data,errors);
	}
	if(!assumeInIce && knownDOMs.empty()){
		std::cerr << "Error: a DOM listing (nicknames file) must be specified using the -L flag"
		"to determine which DOMs are InIce and which are IceTop" << std::endl;
		return(1);
	}
	
	//run vetting checks on the HV-Gain portion of the calibrations
	DOMCalChecker checker(errors);
	
	const overrideSettings noOverride;
	for(std::map<uint64_t,DOMCalRecord>::const_iterator domIt=data.begin(), domEnd=data.end();
		domIt!=domEnd; domIt++){
		//check for overrides on this DOM. . .
		std::map<uint64_t,overrideSettings>::const_iterator overrideIt=overrides.find(domIt->first);
		const overrideSettings& override=((overrideIt!=overrides.end())?overrideIt->second:noOverride);
		
		checker.checkData(domIt->first,domIt->second);
		//skip this DOM if the calibration has fatal errors _unless_ we have an override and that override
		//specifies all necessary settings
		if(errors.domHasFatalError(domIt->first) && !override.full()){
			std::cerr << mainboardID(domIt->first)
			<< " has fatal errors; cannot generate settings" << std::endl;
			continue;
		}
		//this is suboptimal, but rummage around through the errors to find out if
		//this DOM was flagged for anything related to its HV-Gain calibration
		bool hvGainProblems=false;
		if(errors.errors.count(domIt->first)){ //there are some errors on this DOM
			const std::vector<error>& domErrors=errors.errors[domIt->first];
			for(std::vector<error>::const_iterator errIt=domErrors.begin(), end=domErrors.end(); errIt!=end; errIt++){
				if(errIt->message.find("Missing high voltage/gain calibration")!=std::string::npos)
					hvGainProblems=true;
				else if(errIt->message.find("HV Gain")!=std::string::npos)
					hvGainProblems=true;
				else if(errIt->message.find("HV for gain of 10^7 out of range")!=std::string::npos)
					hvGainProblems=true;
			}
		}
		if(hvGainProblems && (!override.minHVSet || !override.maxHVSet)){
			std::cerr << mainboardID(domIt->first)
			<< " has problems with the HV-Gain calibration; settings may be suspect" << std::endl;
		}
		
		std::map<uint64_t,domIdentifier>::const_iterator idIt=knownDOMs.find(domIt->first);
		if(idIt==knownDOMs.end() && !assumeInIce){
			std::cerr << mainboardID(domIt->first)
			<< " was not found in the DOM listing; cannot generate settings" << std::endl;
			continue;
		}
		
		//determine the target gain, depending on whther the DOM is part of IceTop
		//and whether we have been told to treat all DOMs as INIce
		double logGainTarget=(assumeInIce || !probablyIceTop(idIt->second.omkey)?iniceLogGainTarget:icetopLogGainTarget);
		
		const DOMCalRecord& data=domIt->second;
		float targetHV = pow(pow(10,logGainTarget-data.hv_gain_calib.y_intercept),1./data.hv_gain_calib.slope);
		//if an override is set for a voltage limit, use that, otherwise compute a value
		//above or below the value for the target gain, and then limit it to make sure
		//it's in the recommended safe range
		float minHV=(override.minHVSet?override.minHV:limitHV(targetHV-minHVRange/2));
		float maxHV=(override.maxHVSet?override.maxHV:limitHV(targetHV+minHVRange/2));
		
		//if possible we would like to make sure that the range of voltages covered isn't too small
		expandHVRange(minHV,maxHV,override);
		
		int calATWD=(override.atwdSet?override.atwd:defaultCalATWD);
		
		std::cout << mainboardID(domIt->first) << ' ' << (int)(minHV+.5) << ' ' << (int)(maxHV+.5) << ' ' << calATWD << '\n';
	}
	//sweep up any DOMs for which we found override information but no calibration
	for(std::map<uint64_t,overrideSettings>::const_iterator overrideIt=overrides.begin(), end=overrides.end(); overrideIt!=end; overrideIt++){
		if(data.count(overrideIt->first)) //if there is calibration data this DOM was already handled above
			continue;
		const overrideSettings& override=overrideIt->second;
		if(!override.full()){
			std::cerr << mainboardID(overrideIt->first) << " has a partial override but no calibration; "
			 << "cannot generate settings" << std::endl;
			continue;
		}
		std::cout << mainboardID(overrideIt->first) << ' ' << (int)(override.minHV) << ' ' << (int)(override.maxHV) << ' ' << override.atwd << '\n';
	}
	return(0);
}
