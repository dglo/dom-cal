# Magic to do the build from the build directory, as though it were in the source directory
# This will fail horribly if there is whitespace in the current directory path
ifneq ($(notdir $(CURDIR)),build)
include jump.mk
else
VPATH = $(BASEDIR)

PROGRAM=domcal_verify
SRCS = $(shell find $(BASEDIR) -not -name ".*" -name '*.cpp' -print0 | xargs -0 -n 1 basename)
OBJS := $(SRCS:.cpp=.o)
DEPFILES := $(SRCS:.cpp=.P)

BOOST_VERSION = $(shell ls -1 $(I3_PORTS)/lib/ | sed -n 's|boost-\([0-9.]*\)|\1|p')

# use clang as the C++ compiler if it is available and its version is >= 3.0
CLANG_PATH = $(shell which clang++ 2> /dev/null)
ifneq ($(strip $(CLANG_PATH)),)
	CLANG_VERSION_OK = $(shell echo `clang++ -v 2>&1 | sed -n 's|clang version \([0-9]*\.[0-9]*\).*|\1|p'` '>= 3' | bc)
	ifeq ($(CLANG_VERSION_OK),1)
		CXX=clang++
	endif
endif

CXXFLAGS+=-I../../../private/dom-cal/
CXXFLAGS+=-I$(I3_PORTS)/include/boost-$(BOOST_VERSION)/
CXXFLAGS+=`xml2-config --cflags`

CXXFLAGS+=-O2

LDFLAGS+=-L$(I3_PORTS)/lib/boost-$(BOOST_VERSION)/
LDFLAGS+=-lboost_system-mt
LDFLAGS+=-lboost_filesystem-mt
LDFLAGS+=-lboost_regex-mt
LDFLAGS+=`xml2-config --libs`

MAKEDEPEND = gcc -MM -MF $*.d $(CXXFLAGS) $<

.PHONY: all clean

all: $(PROGRAM)

clean: 
	@echo "Removing dependency files"
	@rm -rf $(DEPFILES)
	@echo "Removing object files"
	@rm -f $(OBJS)
	@echo "Removing product"
	@rm -f $(PROGRAM)

#handle dependiecies automatically using Tom Tromey's method, as described by Paul D. Smith
%.o : %.cpp
	@$(MAKEDEPEND); \
            cp $*.d $*.P; \
            sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
                -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
            rm -f $*.d
	@echo Compiling $(<F)
	@$(CXX) $(CXXFLAGS) -c $(INCLUDE) -o $@ $<

-include $(SRCS:.cpp=.P)

$(PROGRAM): $(OBJS)
	@echo Compiling $(PROGRAM)
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $(PROGRAM)

endif # close build directory check