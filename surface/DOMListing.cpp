#include "DOMListing.h"

int domIdentifier::toroidType() const{
	if(domID.size()<3)
		return(-1);
	//Some special DOMs have names which end in 'DOM'. 
	//They (approximately) have new type toroids
	if(name.size()>=3 && name.find("DOM",name.size()-3)==name.size()-3)
		return(1);
	// The third character of regular DOM IDs is the production year, 
	// and DOMs produced in year 6 and later have new type toroids. 
	// However, there are five DOMs which have new toroids, despite 
	// having IDs indicating production before year 6: 
	// Alaska - UP5P0970
	// Beren - UP5H9214
	// Huddinge - TP4H0049
	// Leukophobia - UP5P0672
	// Pipe_Nebula - TP5P0901
	if(domID=="UP5P0970" || domID=="UP5H9214" || domID=="TP4H0049" || domID=="UP5P0672" || domID=="TP5P0901")
		return(1);
	char year=domID[3];
	if(year>='6')
		return(1);
	return(0);
}

void loadDOMListing(std::istream& is, domListing& knownDOMs){
	std::string line;
	getline(is,line); //skip the first input line
	std::pair<uint64_t,domIdentifier> p;
	is.setf(std::ios::hex,std::ios::basefield);
	while(is){
		is >> p.first >> p.second.domID >> p.second.name >> p.second.omkey;
		getline(is,line); //ignore the rest of the line
		knownDOMs.insert(p);
	}
}
