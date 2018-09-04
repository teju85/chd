CXX      := g++
CXXFLAGS := --std=c++11
EXE      := chd
SOURCES  := chd.cpp
ifeq ($(DEBUG),1)
    CXXFLAGS += -g -pg
else
    CXXFLAGS += -O3
endif

default:
	@echo "make what? Available targets are:"
	@echo "  . $(EXE)   - build the exe"
	@echo "  . clean    - clean up the exe and other build files"
	@echo "Flags:"
	@echo "  . DEBUG    - enable a debug build"

$(EXE): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(EXE)
