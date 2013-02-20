#ifndef LIBXMLUTILS_H_INCLUDED
#define LIBXMLUTILS_H_INCLUDED

#include <cassert>
#include <string>
#include <stdexcept>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <iostream>

//a knockoff boost::lexical_cast
#include <sstream>
#include <typeinfo>
class bad_lexical_cast:public std::bad_cast{
public:
	bad_lexical_cast(){}
	virtual ~bad_lexical_cast() throw(){}
	virtual const char* what() const throw(){ return("Bad lexical_cast"); }
};
template<typename Target, typename Source>
Target lexical_cast(const Source& s){
	std::stringstream ss;
	ss << s;
	Target t;
	ss >> t;
	if(ss.fail())
		throw bad_lexical_cast();
	return(t);
}

//a knockoff of boost::shared_ptr, lacking many of its features
template <typename T, typename FreeArgType=T>
class raiiHandle{
private:
	T* ptr;
	void (*cleanup)(FreeArgType*);
	raiiHandle<T,FreeArgType>(const raiiHandle<T,FreeArgType>&);
	const raiiHandle<T,FreeArgType>& operator=(const raiiHandle<T,FreeArgType>&);
public:
	raiiHandle<T,FreeArgType>(T* t=0, void (*c)(FreeArgType*)=NULL):
	ptr(t),cleanup(c){}
	~raiiHandle<T,FreeArgType>(){
		if(cleanup)
			cleanup(ptr);
		else
			delete ptr;
	}
	T& operator*() const{
        assert(ptr!=0 && "Attempt to dereference NULL pointer");
        return(*ptr);
    }
    T* operator->() const{
        assert(ptr!=0 && "Attempt to dereference NULL pointer");
        return(ptr);
    }
	T* get() const{
		return(ptr);
	}
	typedef T*const* bool_type;
	operator bool_type() const{
        return(!ptr?0:&ptr);
    }
};

struct xmlParseError : public std::runtime_error{
public:
	xmlParseError(const std::string& m):std::runtime_error(m){}
};

std::string trim(std::string s,const std::string& whitespace=" \t\n\r");

bool hasAttribute(xmlNode* node, const std::string& name);

template <typename T>
T getAttribute(xmlNode* node, const std::string& name){
	raiiHandle<xmlChar,void> attr(xmlGetProp(node,(const xmlChar*)(name.c_str())),xmlFree);
	if(!attr)
		throw xmlParseError("Node has no attribute '"+name+"'");
	try{
		return(lexical_cast<T>(trim((const char*)attr.get())));
	}catch(bad_lexical_cast& blc){
		throw xmlParseError("Failed to parse attribute '"+name+"' as type "+typeid(T).name()+" (value was "+(char*)attr.get()+")");
	}
}

//specialization for strings
template<>
std::string getAttribute<std::string>(xmlNode* node, const std::string& name);

template <typename T>
T getNodeContents(xmlNode* node){
	raiiHandle<xmlChar,void> content(xmlNodeGetContent(node),xmlFree);
	try{
		return(lexical_cast<T>(trim((const char*)content.get())));
	}catch(bad_lexical_cast& blc){
		throw xmlParseError(std::string("Failed to parse node contents as type ")+typeid(T).name()+" (value was "+(char*)content.get()+")");
	}
}

//specialization for strings
template<>
std::string getNodeContents<std::string>(xmlNode* node);

xmlNode* firstChild(xmlNode* node, const std::string& name="", bool notFoundError=true);
xmlNode* nextSibling(xmlNode* node, const std::string& name="");

#endif