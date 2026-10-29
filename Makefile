CXX = g++
CXXFLAGS = -Wall -std=c++11
TARGET = myshell

SRCS = main.cpp cli.cpp process.cpp pipeline.cpp signals.cpp procinfo.cpp mytree.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp shell.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	bash test_script.sh

clean:
	rm -f *.o $(TARGET) out.txt
