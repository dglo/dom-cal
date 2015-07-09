#ifndef DOMLISTING_H_INCLUDED
#define DOMLISTING_H_INCLUDED

#include <istream>
#include <map>
#include <string>
#include <stdint.h>

struct domIdentifier{
	std::string omkey;
	std::string domID;
	std::string name;
	
	int toroidType() const;
};

typedef std::map<uint64_t,domIdentifier> domListing;

void loadDOMListing(std::istream& is, domListing& knownDOMs);

struct mainboardID{
	uint64_t id;
	mainboardID(uint64_t i):id(i){}
};

std::ostream& operator<<(std::ostream&, const mainboardID&);

#endif //include guard
