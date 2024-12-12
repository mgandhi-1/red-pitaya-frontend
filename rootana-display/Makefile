# Set paths
MIDAS_INCLUDE = $(MIDASSYS)/include
MIDAS_LIB = $(MIDASSYS)/lib
ROOTANA_INCLUDE = $(ROOTANASYS)/include
ROOTANA_LIB = $(ROOTANASYS)/lib
ROOT_INCLUDE = $(ROOTSYS)/include
ROOT_LIB = $(ROOTSYS)/lib

# Compiler and flags
CXX = g++
CXXFLAGS = -I$(MIDAS_INCLUDE) -I$(ROOTANA_INCLUDE) -I$(ROOT_INCLUDE) -g -O2 -Wall -Wuninitialized -DHAVE_LIBZ -DHAVE_ROOT_HTTP -DHAVE_THTTP_SERVER -DHAVE_MIDAS -Dextname 
LDFLAGS = -L$(MIDAS_LIB) -L$(ROOTANA_LIB) -L/home/mgandhi/packages/rootana/libAnalyzer -L$(ROOT_LIB) -lmanalyzer -lrootana -lmidas -lz -lmfe -lCore -lRIO -lNet -lHist -lGpad -lGui -lThread -lpthread -lutil -lrt -lRHTTP 

# Target and sources
TARGET = realtimeplot
SRC = main.cxx TRedPitayaCanvas.cxx
OBJ = $(SRC:.cxx=.o)

# Build target
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

# Build object files
%.o: %.cxx
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(TARGET) $(OBJ)


