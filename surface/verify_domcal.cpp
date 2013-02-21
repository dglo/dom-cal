#include <fstream>
#include <iomanip>
#include <iostream>

#include "DOMCalChecker.h"
#include "DOMListing.h"

int main(int argc, char* argv[]){
	std::string listingFilePath="~/nicknames.txt";
	if(argc==1){
		std::cerr << "Usage: domcal_verify [-L DOM listing file] input_directories" << std::endl;
		return(1);
	}
	std::map<uint64_t,DOMCalRecord> data;
	errorListing errors;
	
	int firstArg=1;
	if(argv[1]==std::string("-L") && argc>2){
		listingFilePath=argv[2];
		firstArg=3;
	}
	for(unsigned int i=firstArg; i<argc; i++)
		parseAllDOMCalFiles(argv[i],data,errors);
	
	domListing knownDOMs;
	std::ifstream listingFile(listingFilePath.c_str());
	if(!listingFile)
		std::cerr << "Unable to read DOM list file; DOM names will be omitted" << std::endl;
	else
		loadDOMListing(listingFile,knownDOMs);
	
	unsigned long failCount=0;
	DOMCalChecker checker(errors);
	for(std::map<uint64_t,DOMCalRecord>::const_iterator domIt=data.begin(), domEnd=data.end();
		domIt!=domEnd; domIt++){
		bool pass=true;
		
		if(!errors.domHasFatalError(domIt->first))
			checker.checkData(domIt->first,domIt->second);
		std::map<uint64_t,std::vector<error> >::const_iterator errorEntry=errors.errors.find(domIt->first);
		
		if(errorEntry!=errors.errors.end()){
			for(std::vector<error>::const_iterator errIt=errorEntry->second.begin(), errEnd=errorEntry->second.end();
				pass && errIt!=errEnd; errIt++){
				if(errIt->severity>=ERROR)
					pass=false;
			}
		}
		
		std::cout << mainboardID(domIt->first);
		unsigned int nameLen=0;
		//std::map<uint64_t,std::string>::const_iterator nameIt=nameMap.find(domIt->first);
		//if(nameIt!=nameMap.end()){
		domListing::const_iterator nameIt=knownDOMs.find(domIt->first);
		if(nameIt!=knownDOMs.end()){
			nameLen=3+nameIt->second.name.size();
			std::cout << " (" << nameIt->second.name << ')';
		}
		std::cout << std::setfill('.') << std::right << std::setw(60-12-nameLen-4) << (pass?"PASS":"FAIL") << std::endl;
		
		if(errorEntry!=errors.errors.end()){
			for(std::vector<error>::const_iterator errIt=errorEntry->second.begin(), errEnd=errorEntry->second.end();
				errIt!=errEnd; errIt++){
				std::cout << ' ' << *errIt << std::endl;
			}
		}
		if(!pass)
			failCount++;
	}
	std::cout << data.size() << " DOM";
	if(data.size()!=1)
		std::cout << 's';
	std::cout << "; " << (data.size()-failCount) << " pass";
	if((data.size()-failCount)==1)
		std::cout << "es";
	std::cout << ", " << failCount << " fail";
	if(failCount==1)
		std::cout << 's';
	std::cout << std::endl;
	return(0);
}
