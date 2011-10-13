#include "libXMLUtils.h"

static class stupidCleanup_t{
public:
	~stupidCleanup_t(){
		xmlCleanupParser();
	}
} cleanupLibXMLStupidity;

std::string trim(std::string s,const std::string& whitespace){
	s=s.erase(s.find_last_not_of(whitespace)+1);
	return(s.erase(0,s.find_first_not_of(whitespace)));
}

bool hasAttribute(xmlNode* node, const std::string& name){
	xmlAttr* attr=xmlHasProp(node,(const xmlChar*)name.c_str());
	return(attr!=NULL);
}

xmlNode* firstChild(xmlNode* node, const std::string& name, bool notFoundError){
	if(!node)
		throw xmlParseError("Unable to search children of NULL XML element");
	for(xmlNode* child=node->children; child!=NULL; child=child->next){
		if(child->type!=XML_ELEMENT_NODE)
			continue;
		if(name.empty() || (char*)child->name==name)
			return(child);
	}
	if(notFoundError){
		if(name.empty())
			throw xmlParseError(std::string("Node '")+(char*)node->name+"' has no children");
		throw xmlParseError(std::string("Node '")+(char*)node->name+"' has no child with name '"+name+"'");
	}
	return(NULL);
}

xmlNode* nextSibling(xmlNode* node, const std::string& name){
	if(!node)
		throw xmlParseError("Unable to search siblings of NULL XML element");
	for(node=node->next; node; node=node->next){
		if(node->type!=XML_ELEMENT_NODE)
			continue;
		if(name.empty() || (char*)node->name==name)
			return(node);
	}
	return(NULL);
}

template<>
std::string getAttribute<std::string>(xmlNode* node, const std::string& name){
	boost::shared_ptr<xmlChar> attr(xmlGetProp(node,(const xmlChar*)(name.c_str())),xmlFree);
	if(!attr)
		throw xmlParseError("Node has no attribute '"+name+"'");
	return(trim((const char*)attr.get()));
}

template<>
std::string getNodeContents<std::string>(xmlNode* node){
	boost::shared_ptr<xmlChar> content(xmlNodeGetContent(node),xmlFree);
	return(trim((const char*)content.get()));
}
