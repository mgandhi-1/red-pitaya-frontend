# Compiler
CXX = g++

# Included directories
INCLUDES = -I$(MIDASSYS)/include

# Linked directories
LIBS = -L$(MIDASSYS)/lib -lmidas -lz -lutil -lpthread -lmfe

# Compilation flags
CXXFLAGS = $(INCLUDES) -g -O0 -Wall -Wextra -fsanitize=address -Wno-unused-parameter -Wuninitialized -Wpointer-arith -Wcast-qual -Wstrict-aliasing -Wformat-security -Wmissing-declarations -Wmissing-include-dirs -Wswitch-enum -Wconversion -pedantic -fno-inline
LDFLAGS = $(LIBS) -fsanitize=address

# Target executable
TARGET = frontend

# Source files
SRCS = frontend.cxx

# Objects
OBJS = $(SRCS:.cxx=.o)

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cxx
	$(CXX) -c $< -o $@  $(CXXFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)
