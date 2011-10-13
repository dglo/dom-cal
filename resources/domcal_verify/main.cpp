#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>

#include <boost/regex.hpp>

#include "DOMCalParser.h"
#include "DOMCalChecker.h"

std::map<uint64_t,std::string> readDOMNames(const std::string& filename){
	std::map<uint64_t,std::string> nameMap;
	std::ifstream infile(filename.c_str());
	std::string line;
	uint64_t mbID;
	const boost::regex rgex("^([0-9A-Fa-f]{12})\\s+[A-Za-z0-9]+\\s+([A-Za-z0-9_]+)\\s+(-|([0-9]+-[0-9]+)).*");
	//skip the first line
	getline(infile,line);
	while(getline(infile,line)){
		boost::smatch rMatch;
		if(!boost::regex_match(line, rMatch, rgex) || rMatch.size()<4)
			continue;
		std::string idStr(rMatch[1]);
		std::istringstream ss(idStr);
		ss >> std::hex >> mbID;
		nameMap.insert(std::make_pair(mbID,rMatch[3]+" "+rMatch[2]));
	}
	return(nameMap);
}

int main(int argc, char* argv[]){
	if(argc==1){
		std::cerr << "Usage: domcal_verify input_directories" << std::endl;
		return(1);
	}
	std::map<int64_t,DOMCalRecord> data;
	errorListing errors;
	for(unsigned int i=1; i<argc; i++)
		parseAllDOMCalFiles(argv[i],data,errors);
	std::map<uint64_t,std::string> nameMap(readDOMNames("/home/krasberg/nicknames.txt"));
	
	unsigned long failCount=0;
	DOMCalChecker checker(errors);
	for(std::map<int64_t,DOMCalRecord>::const_iterator domIt=data.begin(), domEnd=data.end();
		domIt!=domEnd; domIt++){
		bool pass=true;
		
		checker.checkData(domIt->first,domIt->second);
		std::map<uint64_t,std::vector<error> >::const_iterator errorEntry=errors.errors.find(domIt->first);
		
		if(errorEntry!=errors.errors.end()){
			for(std::vector<error>::const_iterator errIt=errorEntry->second.begin(), errEnd=errorEntry->second.end();
				pass && errIt!=errEnd; errIt++){
				if(errIt->severity>=ERROR)
					pass=false;
			}
		}
		
		std::cout << std::hex << std::setfill('0') << std::right << std::setw(12) << domIt->first << std::dec;
		unsigned int nameLen=0;
		std::map<uint64_t,std::string>::const_iterator nameIt=nameMap.find(domIt->first);
		if(nameIt!=nameMap.end()){
			nameLen=3+nameIt->second.size();
			std::cout << " (" << nameIt->second << ')';
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
