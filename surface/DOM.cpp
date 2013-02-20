#include "DOM.h"

const char* DOM::endlManipulator::endlStr="\r\n";
DOM::endlManipulator DOM::endl;

std::ostream& operator<<(std::ostream& os, const DOM::endlManipulator&){
	os << DOM::endlManipulator::endlStr;
	os.flush();
	return(os);
}
std::string operator+(const std::string s, const DOM::endlManipulator&){
	return(s+DOM::endlManipulator::endlStr);
}
std::string operator+(const DOM::endlManipulator&, const std::string s){
	return(DOM::endlManipulator::endlStr+s);
}