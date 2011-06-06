CXXFLAGS=-DNO_STORAGE -Wall -DDEBUG_BUILD -m32
OPTFLAGS=-O3  
CC=gcc
CXX=g++

ifdef DEBUG
ifeq ($(DEBUG), 1)
OPTFLAGS= -O0 -g -m32
endif
endif
CXXFLAGS+=$(OPTFLAGS) -m32

EXE_NAME=DRAMSim
LIB_NAME=libdramsim.so

SRC = $(wildcard *.cpp)
OBJ = $(addsuffix .o, $(basename $(SRC)))

#build portable objects (i.e. with -fPIC)
POBJ = $(addsuffix .po, $(basename $(SRC)))

REBUILDABLES=$(OBJ) ${POBJ} $(EXE_NAME) $(LIB_NAME) 

all: ${EXE_NAME}

#   $@ target name, $^ target deps, $< matched pattern
$(EXE_NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ 
	@echo "Built $@ successfully" 

$(LIB_NAME): $(POBJ)
	$(CXX) -g -shared -Wl,-soname,$@ -o $@ $^ -m32
	@echo "Built $@ successfully"

#include the autogenerated dependency files for each .o file
-include $(OBJ:.o=.dep)
-include $(POBJ:.po=.dep)

# build dependency list via gcc -M and save to a .dep file
%.dep : %.cpp
	@$(CXX) -M $(CXXFLAGS) $< > $@

# build all .cpp files to .o files
%.o : %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

#po = portable object .. for lack of a better term
%.po : %.cpp
	$(CXX) $(CXXFLAGS) -DLOG_OUTPUT -fPIC -o $@ -c $<

clean: 
	rm -f $(REBUILDABLES) *.dep
