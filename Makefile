# Compiler and flags
CXX = g++
CXXFLAGS = -lz -lpthread

# Directories
LIBS = /usr/local/lib/libmfe.a /usr/local/lib/libmidas.a

# Target executable
TARGET = frontend

# Source files
SRC = frontend.cxx

# Build target
$(TARGET): $(SRC)
	$(CXX) -o $(TARGET) $(SRC) $(LIBS) $(CXXFLAGS)

# Clean up generated files
clean:
	rm -f $(TARGET)
