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

#endif //include guard
