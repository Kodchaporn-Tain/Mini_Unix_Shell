CXX = g++
CXXFLAGS = -Wall -std=c++11
TARGET = myshell

# เพิ่ม mytree.cpp เข้าไปในบรรทัดนี้แล้ว
SRCS = main.cpp cli.cpp process.cpp pipeline.cpp signals.cpp procinfo.cpp mytree.cpp
OBJS = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)