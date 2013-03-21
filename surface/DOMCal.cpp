#include <cassert>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <vector>

#include <pthread.h>

#include "DOM.h"
#include "DOMListing.h"

//Surface software version
const unsigned int surfaceMajorVersion = 1;
const unsigned int surfaceMinorVersion = 0;
const unsigned int surfacePatchVersion = 2;

//The components of the version of the in-ice software
//with which this version of the surface software expects to work
const unsigned int expectedMajorVersion = 7;
const unsigned int expectedMinorVersion = 6;
const unsigned int expectedPatchVersion = 0;

struct DOMCalSettings{
	unsigned short cardNumber;
	unsigned short pairNumber;
	char domLabel;
	std::string outputDir;
	bool doHVCal;
	bool iterateHV;
	int minHV;
	int maxHV;
	int calATWD;
	std::string mbID;
	int toroidType;
	
	DOMCalSettings():
	doHVCal(false),
	iterateHV(false),
	minHV(0),maxHV(0)
	{}
};

std::ostream& operator<<(std::ostream& os, const DOMCalSettings& settings){
	os << "Will calibrate DOM " << settings.mbID 
	<< " located at dhc" << settings.cardNumber << 'w' << settings.pairNumber << 'd' << settings.domLabel 
	<< " with toroid type " << settings.toroidType << " and ATWD " << settings.calATWD;
	if(settings.doHVCal){
		os << " with HV between " << settings.minHV << " and " << settings.maxHV;
		if(settings.iterateHV)
			os << " and iterating measurements";
	}
	else
		os << " without using HV";
	return(os);
}

//helper function which just reads the contents of a file
//this is only intended for use on the very short (single line)
//files 
std::string getFileContents(const std::string& path){
	std::string data;
	std::ifstream file(path.c_str());
	if(!file.good())
		throw std::runtime_error("Unable to read "+path);
	std::string line;
	while(getline(file,line)){
		if(!data.empty())
			data+="\n";
		data+=line;
	}
	return(data);
}

bool fileExists(const std::string& path){
	std::ifstream file(path.c_str());
	return file.good();
}

bool endsWith(const std::string& str, const std::string& suffix){
	if(suffix.size()>str.size())
		return(false);
	return(str.rfind(suffix)==(str.size()-suffix.size()));
}

void trimWhitespace(std::string& s){
	static const char* whitespace=" \n\r\t\v";
	size_t pos;
	if((pos=s.find_first_not_of(whitespace))!=0)
		//if there are no non-whitespace characters pos==std::string::npos, so the entire line is erased
		s.erase(0,pos);
	if(!s.empty() && (pos=s.find_last_not_of(whitespace))!=s.size()-1)
		s.erase(pos+1);
}

//Find all DOMs which are connected to this hub
//In oreder for this function to have access to the DOM's mainboard IDs the DOMs must have gone from configboot to iceboot!
//Returns a map of mainboard IDs to settings objects, where the settings will contain only
//the information necessary to logically locate the DOM (card number, pair number, and label);
//all actual calibration settings will have to be filled in from another source.
std::map<uint64_t,DOMCalSettings> determineConnectedDOMs(){
	std::map<uint64_t,DOMCalSettings> foundDOMs;
	for(unsigned short cardNumber=0; cardNumber<8; cardNumber++){
		//skip the card if it isn't plugged in
		if(!fileExists(DOM::cardDirPath(cardNumber)))
			continue;
		
		for(unsigned short pairNumber=0; pairNumber<4; pairNumber++){
			//skip the pair if it isn't plugged in
			if(!endsWith(getFileContents(DOM::pairDirPath(cardNumber,pairNumber)+"/is-plugged"),"is plugged in."))
				continue;
			
			for(char domLabel='A'; domLabel<='B'; domLabel++){
				std::string domDirPath=DOM::domDirPath(cardNumber,pairNumber,domLabel);
				//skip the DOM if it isn't talking to us
				if(!endsWith(getFileContents(domDirPath+"/is-communicating"),"is communicating"))
					continue;
				//if the DOM is still in configboot we can't do anything with it,
				//so complain and skip
				if(endsWith(getFileContents(domDirPath+"/is-not-configboot"),"is not out of configboot")){
					std::cerr << "The DOM at " << cardNumber << pairNumber << domLabel
					  << " is still in configboot, so its identity cannot be determined" << std::endl;
					continue;
				}
				
				//attempt to read and validate the DOM's mainbaord ID
				std::string idData=getFileContents(domDirPath+"/id");
				std::size_t pos=idData.rfind(' ');
				if(pos==std::string::npos || pos==idData.size()-1){
					std::cerr << "Unexpected data format in " << domDirPath << "/id: " << idData << std::endl;
					continue;
				}
				std::string mbidStr=idData.substr(pos+1);
				trimWhitespace(mbidStr);
				std::istringstream mbidStream(mbidStr);
				uint64_t mbid=0;
				mbidStream >> std::hex >> mbid;
				if(mbidStream.fail() || mbid==0){
					std::cerr << "Unrecognized or unset mainboard id in " << domDirPath << "/id: " << mbidStr << std::endl;
					continue;
				}
				
				foundDOMs.insert(std::make_pair(mbid,DOMCalSettings()));
				foundDOMs[mbid].cardNumber=cardNumber;
				foundDOMs[mbid].pairNumber=pairNumber;
				foundDOMs[mbid].domLabel=domLabel;
				foundDOMs[mbid].mbID=mbidStr;
			} //loop over DOMs
		} //loop over pairs
	} //loop over cards
	return(foundDOMs);
}

//check that calibration settings are sane
//small tweaks will be made for consistency
void validateCalibrationSettings(DOMCalSettings& settings){
	if(settings.minHV==0 && settings.maxHV==0)
		settings.doHVCal=false;
	else{
		if(settings.minHV<900){
			std::cerr << '[' << settings.mbID << ']' << " Warning: Specified minimum high voltage (" << settings.minHV << ") will be raised to 900 V" << std::endl;
			settings.minHV=900;
		}
		if(settings.maxHV>2000){
			std::cerr << '[' << settings.mbID << ']' << " Warning: Specified maximum high voltage (" << settings.maxHV << ") will be lowered to 2000 V" << std::endl;
			settings.maxHV=2000;
		}
		if(settings.maxHV<settings.minHV){
			std::cerr << '[' << settings.mbID << ']' << " Warning: Specified maximum high voltage (" << settings.maxHV
			<< ") < Specified minimum high voltage (" << settings.minHV << "); lowering minimum to " << settings.maxHV << " V" << std::endl;
			settings.minHV=settings.maxHV;
		}
	}
	if(!settings.doHVCal)
		settings.iterateHV=false;
	
	if(settings.calATWD<-1 || settings.calATWD>1)
		throw std::runtime_error("Invalid preferred calibration ATWD number");
}

//read settings from a file to fill in the calibration settings for the DOMs known to be connected to this hub
//any DOM which has no entry in the settings file will be pruned from the foundDOMs list,
//on the assumption that if we don't have settings we should attempt to calibrate that DOM
void parseDOMSettingsFile(const std::string& settingFilePath, std::map<uint64_t,DOMCalSettings>& foundDOMs, const std::string& outputDir, bool useHV, bool iterate, const std::map<uint64_t,domIdentifier>& knownDOMs){
	//tag DOMs as needing their setting completed (using the calATWD field)
	const int needSettings=-255; //some dummy value which must not be a valid value for an ATWD choice {-1,0,1}
	for(std::map<uint64_t,DOMCalSettings>::iterator it=foundDOMs.begin(), end=foundDOMs.end(); it!=end; it++)
		it->second.calATWD=needSettings;
	
	std::ifstream settingsFile(settingFilePath.c_str());
	if(!settingsFile)
		throw std::runtime_error("Unable to read "+settingFilePath);
	//read the file
	uint64_t mbid;
	double minHV, maxHV;
	int calATWD;
	while(settingsFile >> std::hex >> mbid >> std::dec >> minHV >> maxHV >> calATWD){
		if(settingsFile.eof())
			break;
		std::map<uint64_t,DOMCalSettings>::iterator it=foundDOMs.find(mbid);
		if(it==foundDOMs.end())
			continue;
		
		DOMCalSettings& settings=it->second;
		settings.outputDir=outputDir;
		settings.doHVCal=useHV;
		settings.iterateHV=iterate;
		settings.minHV=(int)(minHV+0.5); //round floating point values to nearest integers
		settings.maxHV=(int)(maxHV+0.5);
		settings.calATWD=calATWD;
		
		validateCalibrationSettings(settings);
		
		std::map<uint64_t,domIdentifier>::const_iterator nameIt=knownDOMs.find(it->first);
		if(nameIt==knownDOMs.end()){
			std::ostringstream ss;
			ss << "DOM " << settings.mbID << " not found in mainboard ID/DOM ID listing";
			throw std::runtime_error(ss.str());
		}
		settings.toroidType=nameIt->second.toroidType();
		if(settings.toroidType==-1)
			throw std::runtime_error("Unable to determine DOM's toroid type");
	}
	
	//remove all DOMs from the calibration list which are still tagged as needing settings,
	//since they apparently were not listed in the settings file
	for(std::map<uint64_t,DOMCalSettings>::iterator next=foundDOMs.begin(), end=foundDOMs.end(), it=(next==end?next:next++);
		it!=end; it=(next==end?next:next++)){
		if(it->second.calATWD==needSettings){
			std::cerr << "Dropping DOM " << mainboardID(it->first)
			  << " from calibration list, no settings for it were found" << std::endl;
			foundDOMs.erase(it);
		}
	}
}

//read the settings for a single DOM spcified as a '-D' argument
DOMCalSettings parseDOMSettingsArg(const std::string& argument, const std::string& outputDir,
                                   bool useHV, bool iterate, const std::map<uint64_t,domIdentifier>& knownDOMs){
	DOMCalSettings settings;
	settings.outputDir=outputDir;
	settings.doHVCal=useHV;
	settings.iterateHV=iterate;
	std::istringstream ss(argument);
	char dummy;
	ss >> dummy >> dummy; //skip the -D
	
	ss >> settings.cardNumber >> dummy;
	if(!ss)
		throw std::runtime_error("Missing or malformed card number");
	
	ss >> settings.pairNumber >> dummy;
	if(!ss)
		throw std::runtime_error("Missing or malformed pair number");
	
	ss >> settings.domLabel >> dummy;
	if(!ss)
		throw std::runtime_error("Missing or malformed DOM label");
	
	uint64_t mbID;
	ss >> std::hex >> mbID >> std::dec >> dummy;
	if(!ss)
		throw std::runtime_error("Missing or malformed mainboard ID number");
	std::ostringstream mbStr;
	mbStr << mainboardID(mbID);
	settings.mbID=mbStr.str();
	
	ss >> settings.minHV >> dummy;
	if(!ss)
		throw std::runtime_error("Missing or malformed minimum high voltage");
	
	ss >> settings.maxHV >> dummy;
	if(!ss)
		throw std::runtime_error("Missing or malformed maximum high voltage");
	
	ss >> settings.calATWD;
	if(!ss)
		throw std::runtime_error("Missing or malformed preferred calibration ATWD number");
	
	validateCalibrationSettings(settings);
	
	std::map<uint64_t,domIdentifier>::const_iterator it=knownDOMs.find(mbID);
	if(it==knownDOMs.end())
		throw std::runtime_error("DOM not found in mainboard ID/DOM ID listing");
	settings.toroidType=it->second.toroidType();
	if(settings.toroidType==-1)
		throw std::runtime_error("Unable to determine DOM's toroid type");
	
	return(settings);
}

void* runDOMCal(void* arg){
	DOMCalSettings& settings = *static_cast<DOMCalSettings*>(arg);
	try{
		DOM dom(settings.cardNumber,settings.pairNumber,settings.domLabel);
		//touch the .running file
		{
			std::ofstream((settings.outputDir+"/domcal_"+settings.mbID+".xml.running").c_str());
		}
		dom.iceboot();
		dom << "crlf domid type type" << DOM::endl;
		dom.receive("crlf domid type type\r\n");
		std::string id;
		dom >> id;
		trimWhitespace(id);
		if(id!=settings.mbID)
			throw std::runtime_error("DOM reports non-matching mainboard ID number: "+id);
		dom.receive(">");
		
		//attempt to locate the domcal program
		dom << "s\" domcal\" find if ls endif" << DOM::endl;
		std::string domcalLoc=dom.receive(">");
		if(domcalLoc=="s\" domcal\" find if ls endif\r\n>")
			throw std::runtime_error("domcal binary not found");
		
		std::cout << '[' << settings.mbID << ']' << " Starting domcal..." << std::endl;
		//start domcal
		dom << "exec" << DOM::endl;
		dom.receive("exec");
		
		std::string response;
		dom >> response;
		
		if(response=="Welcome"){
			dom >> response;
			assert(response=="to");
			dom >> response;
			assert(response=="domcal");
			dom >> response;
			assert(response=="version");
			int major=-1, minor=-1, patch=1;
			char point;
			dom >> major >> point >> minor >> point >> patch;
			if(!(major==expectedMajorVersion && minor==expectedMinorVersion && patch==expectedPatchVersion)){
				std::ostringstream ss;
				ss << "Version mismatch! Expected version is " << expectedMajorVersion << '.' << expectedMinorVersion << '.' << expectedPatchVersion
				  << " but installed version seems to be " << major << '.' << minor << '.' << patch;
				throw std::runtime_error(ss.str());
			}
			dom >> response;
		}
		
		//get date and time
		time_t rawtime=time(NULL);
		tm now = *gmtime(&rawtime);
		
		//answer the DOM's questions about the date, time, and toroid
		while(response=="Enter"){
			dom >> response;
			if(response=="year"){
				dom.receive(": ");
				dom << now.tm_year+1900 << DOM::endl;
				dom.receive(DOM::endl);
			}
			else if(response=="month"){
				dom.receive(": ");
				dom << now.tm_mon << DOM::endl;
				dom.receive(DOM::endl);
			}
			else if(response=="day"){
				dom.receive(": ");
				dom << now.tm_mday << DOM::endl;
				dom.receive(DOM::endl);
			}
			else if(response=="time"){
				char buf[7];
				strftime(&buf[0],6,"%H%M%S",&now);
				dom.receive(": ");
				dom << buf << DOM::endl;
				dom.receive(DOM::endl);
			}
			else if(response=="toroid"){
				dom.receive(": ");
				dom << settings.toroidType << DOM::endl;
				dom.receive(DOM::endl);
			}
			dom >> response;
		}
		
		//select the primary calibration ATWD
		if(response=="Which")
			dom.receive("ATWD chip should be used for calibrations (0/1, -1 or <return> to select automatically)? ");
		else
			throw std::runtime_error("Unexpected response when ATWD selection expected ("+response+")");
		dom << settings.calATWD << DOM::endl;
		dom.receive(DOM::endl);
		
		//answer the DOM's questions about high voltage
		response=dom.receive("Do you want to perform the HV portion of the calibration (y/n)? ",false);
		if(!settings.doHVCal){
			dom << 'n' << DOM::endl;
			dom.receive(DOM::endl);
		}
		else{
			dom << 'y' << DOM::endl;
			dom.receive(DOM::endl);
			
			response=dom.receive(DOM::endl,false);
			
			dom.receive("Do you want to iterate the gain/HV calibration (y/n)? ");
			
			if(settings.iterateHV)
				dom << 'y' << DOM::endl;
			else
				dom << 'n' << DOM::endl;
			dom.receive(DOM::endl);
			
			dom.receive("Enter minimum HV (0V-2000V): ");
			dom << settings.minHV << DOM::endl;
			dom.receive(DOM::endl);
			
			std::ostringstream maxPrompt;
			dom.receive(maxPrompt.str());
			dom << settings.maxHV << DOM::endl;
			dom.receive(DOM::endl);
		}
		
		std::cout << '[' << settings.mbID << ']' << " Waiting for domcal to finish..." << std::endl;
		//open log file
		std::ofstream logFile((settings.outputDir+"/domcal_"+settings.mbID+".out").c_str());
		while(true){
			response=dom.receive(DOM::endl,false);
			logFile << response << std::endl;
			if(response=="Send compressed XML (y/n)?")
				break;
		}
		
		std::cout << '[' << settings.mbID << ']' << " Receiving XML..." << std::endl;
		//we want compressed xml
		dom << 'y' << DOM::endl;
		dom.receive(DOM::endl);
		//get the XML and put it into a file
		std::ofstream xmlFile((settings.outputDir+"/domcal_"+settings.mbID+".xml").c_str());
		dom.zreceiveToStream(xmlFile);
		
		//remove the .running file
		unlink((settings.outputDir+"/domcal_"+settings.mbID+".xml.running").c_str());
		std::cout << '[' << settings.mbID << ']' << " Done" << std::endl;
	}catch(std::exception& err){
		std::cerr << "Caught runtime error while calibrating DOM " << settings.mbID << ":\n"
		<< err.what() << std::endl;
	}catch(...){
		std::cerr << "Caught unknown error while calibrating DOM " << settings.mbID << std::endl;
	}
	return(NULL);
}

void usage(){
	std::cerr << "Usage: DOMCal \n"
	"   -L [DOM listing file] (providing mappings from mainboard IDs to DOM IDs)\n"
	"   -d [output directory] (where XML results and log files will be written)\n"
	"   -v (allow high voltage to be activated)\n"
	"   -i (iterate high voltage measurements)\n"
    "   -V (print version and exit)\n"
	"   -D[card]:[pair]:[dom]:[mbid]:[min HV]:[max HV]:[preferred ATWD]\n"
	"     Specifies a DOM to calibrate by its DOR card number (0-7), \n"
	"     wire pair number (0-7), and DOM label (A or B). The DOM's\n"
	"     mainboard ID is checked against mbid, min HV and max HV are the \n"
	"     limits on the high voltage to be used (900 V-2000 V), and the \n"
	"     preferred ATWD is 0, 1, or -1, where -1 allows the DOM to \n"
	"     choose automatically. \n"
	"     If both the min and max HV are specified as 0, high voltage will\n"
	"     not be used for that DOM.\n"
	"   -S [settings file] Specify a path to a file which contains calibration\n"
	"     settings for many DOMs. DOMCal will attempt to detect the DOMs actually\n"
	"     attached to the hub on which it is running and calibrate them if entries\n"
	"     for them appear in this file.\n"
	"     The file must contain whitespace separated data with the following columns:\n"
	"     Mainboard ID, Minimum HV, Maximum HV, Preferred ATWD\n"
	"     The meanings of these values are the same as for -D\n"
	" The L, d, v, and i options apply to all DOMs subsequently specified using -D or -S."
	<< std::endl;
}

int main(int argc, char* argv[]){
	std::string outputDir;
	bool useHV=false, iterateHV=false;
	std::map<uint64_t,domIdentifier> knownDOMs; //the names and ID numbers of DOMs
	std::vector<DOMCalSettings> settings;
	
	if(argc<=1){
		usage();
		return(1);
	}
	
	for(int i=1; i<argc; i++){
		std::string arg=argv[i];
		if(arg=="-d"){
			if(i==argc-1){
				std::cerr << "Missing output directory path after -d flag" << std::endl;
				return(1);
			}
			i++;
			outputDir=argv[i];
		}
        else if(arg=="-V") {
            std::cerr << "DOMCal surface software version " << 
              surfaceMajorVersion << "." <<
              surfaceMinorVersion << "." <<
              surfacePatchVersion << std::endl;
            return(0);
        }
		else if(arg=="-v")
			useHV=true;
		else if(arg=="-i")
			iterateHV=true;
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
		else if(arg.find("-D")==0){
			try{
				settings.push_back(parseDOMSettingsArg(arg,outputDir,useHV,iterateHV,knownDOMs));
			} catch(std::runtime_error& err){
				std::cerr << "Error processing argument \"" << arg << "\":\n" << err.what() << std::endl;
				return(1);
			}
		}
		else if(arg=="-S"){
			if(i==argc-1){
				std::cerr << "Missing DOM settings file path after -S flag" << std::endl;
				return(1);
			}
			i++;
			std::map<uint64_t,DOMCalSettings> connectedDOMs=determineConnectedDOMs();; //the DOMs connected to this hub
			parseDOMSettingsFile(argv[i],connectedDOMs,outputDir,useHV,iterateHV,knownDOMs);
			for(std::map<uint64_t,DOMCalSettings>::iterator domIt=connectedDOMs.begin(), end=connectedDOMs.end(); domIt!=end; domIt++)
				settings.push_back(domIt->second);
		}
		else
			std::cerr << "Unrecognized argument/option: " << arg << std::endl;
	}
	
	if(settings.empty()){
		std::cout << "No DOMs to calibrate, exiting" << std::endl;
		return(1);
	}

    std::cout << "DOMCal surface software version " << surfaceMajorVersion << "." <<
        surfaceMinorVersion << "." << surfacePatchVersion << std::endl;
	
	const int nDOMs=settings.size();
	for(int i=0; i<nDOMs; i++)
		std::cout << settings[i] << std::endl;
	//spawn a thread for each DOM being calibrated
	std::vector<pthread_t> threads(nDOMs);
	pthread_attr_t threadAttributes;
	pthread_attr_init(&threadAttributes);
	pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setstacksize(&threadAttributes, 4*1024); //we don't need much stack space
	for(int i=0; i<nDOMs; i++){
		int err=pthread_create(&threads[i],&threadAttributes,&runDOMCal,&settings[i]);
		if(err)
			std::cerr << "failed to start thread for DOM " << settings[i].mbID << std::endl;
	}
	pthread_attr_destroy(&threadAttributes);
	//wait for all calibration threads to finish
	for(int i=0; i<nDOMs; i++)
		pthread_join(threads[i],NULL);
	
	//let's print some statistics :)
	std::string line;
	std::ostringstream statusPath;
	statusPath << "/proc/" << getpid() << "/status";
	std::ifstream statusFile(statusPath.str().c_str());
	std::cout << "Process statistics:\n";
	while(getline(statusFile,line))
		std::cout << line << '\n';
	
	return(0);
}
