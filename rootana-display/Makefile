# Set paths
MIDAS_INCLUDE = $(MIDASSYS)/include
MIDAS_LIB = $(MIDASSYS)/lib
ROOTANA_INCLUDE = $(ROOTANASYS)/include
ROOTANA_LIB = $(ROOTANASYS)/lib
ROOT_INCLUDE = $(ROOTSYS)/include
ROOT_LIB = $(ROOTSYS)/lib

# Compiler and flags
CXX = g++
CXXFLAGS = -I$(MIDAS_INCLUDE) -I$(ROOTANA_INCLUDE) -I$(ROOT_INCLUDE) -g -O2 -Wall -Wuninitialized -DHAVE_LIBZ
LDFLAGS = -L$(MIDAS_LIB) -L$(ROOTANA_LIB) -L$(ROOT_LIB) -lrootana -lmidas -lz -lmfe

# Target and sources
TARGET = realtimeplot
SRC = main.cxx

# Build target
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

# Clean target
clean:
	rm -f $(TARGET)
