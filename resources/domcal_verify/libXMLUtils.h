#ifndef LIBXMLUTILS_H_INCLUDED
#define LIBXMLUTILS_H_INCLUDED

#include <string>
#include <stdexcept>

#include <boost/lexical_cast.hpp>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <boost/shared_ptr.hpp>

struct xmlParseError : public std::runtime_error{
public:
	xmlParseError(const std::string& m):std::runtime_error(m){}
};

std::string trim(std::string s,const std::string& whitespace=" \t\n\r");

bool hasAttribute(xmlNode* node, const std::string& name);

template <typename T>
T getAttribute(xmlNode* node, const std::string& name){
	boost::shared_ptr<xmlChar> attr(xmlGetProp(node,(const xmlChar*)(name.c_str())),xmlFree);
	if(!attr)
		throw xmlParseError("Node has no attribute '"+name+"'");
	try{
		return(boost::lexical_cast<T>(trim((const char*)attr.get())));
	}catch(boost::bad_lexical_cast& blc){
		throw xmlParseError("Failed to parse attribute '"+name+"' as type "+typeid(T).name()+" (value was "+(char*)attr.get()+")");
	}
}

//specialization for strings
template<>
std::string getAttribute<std::string>(xmlNode* node, const std::string& name);

template <typename T>
T getNodeContents(xmlNode* node){
	boost::shared_ptr<xmlChar> content(xmlNodeGetContent(node),xmlFree);
	try{
		return(boost::lexical_cast<T>(trim((const char*)content.get())));
	}catch(boost::bad_lexical_cast& blc){
		throw xmlParseError(std::string("Failed to parse node contents as type ")+typeid(T).name()+" (value was "+(char*)content.get()+")");
	}
}

//specialization for strings
template<>
std::string getNodeContents<std::string>(xmlNode* node);

xmlNode* firstChild(xmlNode* node, const std::string& name="", bool notFoundError=true);
xmlNode* nextSibling(xmlNode* node, const std::string& name="");

#endif