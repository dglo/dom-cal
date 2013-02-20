CXXFLAGS+= -Os -I../private/dom-cal -I/usr/include/libxml2
DOMCal_LDFLAGS= $(LDFLAGS) -lz -pthread
verify_domcal_LDFLAGS= $(LDFLAGS) -lxml2
DOMCalGrapher_LDFLAGS=$(LDFLAGS) -lz -lpng -lxml2
DOMCalConfig_LDFLAGS=$(LDFLAGS) -lxml2

all: DOMCal verify_domcal DOMCalGrapher DOMCalConfig

clean: 
	rm -f DOM.o DOMListing.o DOMCal.o DOMCal
	rm -f SimpleDrawing.o DOMCalGrapher.o DOMCalGrapher
	rm -f XMLUtils.o DOMCalParser.o DOMCalChecker.o verify_domcal
	rm -f DOMCalConfig.o DOMCalConfig

DOMCal: DOMCal.o DOM.o DOMListing.o
	$(CXX) $(CXXFLAGS) $(DOMCal_LDFLAGS) -o DOMCal DOM.o DOMListing.o DOMCal.o

DOM.o: DOM.cpp DOM.h DOMIOBuffer.h
	$(CXX) $(CXXFLAGS) -c -o DOM.o DOM.cpp

DOMListing.o: DOMListing.cpp DOMListing.h

DOMCal.o: DOMCal.cpp DOM.h DOMIOBuffer.h DOMListing.h
	$(CXX) $(CXXFLAGS) -c -o DOMCal.o DOMCal.cpp

verify_domcal: XMLUtils.o DOMListing.o DOMCalParser.o DOMCalChecker.o
	$(CXX) $(CXXFLAGS) $(verify_domcal_LDFLAGS) -o verify_domcal XMLUtils.o DOMListing.o DOMCalParser.o DOMCalChecker.o

XMLUtils.o: XMLUtils.cpp XMLUtils.h
	$(CXX) $(CXXFLAGS) -c -o XMLUtils.o XMLUtils.cpp

DOMCalParser.o: DOMCalParser.cpp DOMCalParser.h XMLUtils.h
	$(CXX) $(CXXFLAGS) -c -o DOMCalParser.o DOMCalParser.cpp

DOMCalChecker.o: DOMCalChecker.cpp DOMCalChecker.cpp DOMListing.h
	$(CXX) $(CXXFLAGS) -c -o DOMCalChecker.o DOMCalChecker.cpp

DOMCalGrapher: DOMCalGrapher.o SimpleDrawing.o DOMCalParser.o XMLUtils.o DOMListing.o
	$(CXX) $(CXXFLAGS) $(DOMCalGrapher_LDFLAGS) -o DOMCalGrapher DOMCalGrapher.o SimpleDrawing.o DOMCalParser.o XMLUtils.o DOMListing.o

DOMCalGrapher.o: DOMCalGrapher.cpp SimpleDrawing.h DOMListing.h DOMCalParser.h XMLUtils.h
	$(CXX) $(CXXFLAGS) -c -o DOMCalGrapher.o DOMCalGrapher.cpp

SimpleDrawing.o: SimpleDrawing.cpp SimpleDrawing.h
	$(CXX) $(CXXFLAGS) -c -o SimpleDrawing.o SimpleDrawing.cpp

DOMCalConfig.o : DOMCalConfig.cpp DOMCalParser.h DOMListing.h XMLUtils.h
	$(CXX) $(CXXFLAGS) -c -o DOMCalConfig.o DOMCalConfig.cpp

DOMCalConfig : DOMCalConfig.o DOMCalParser.o XMLUtils.o
	$(CXX) $(CXXFLAGS) $(DOMCalConfig_LDFLAGS) -o DOMCalConfig DOMCalConfig.o DOMCalParser.o DOMListing.o XMLUtils.o