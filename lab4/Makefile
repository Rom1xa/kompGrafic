CXX = g++
CXXFLAGS = -Wall -Wextra
LDFLAGS = -lglut -lGL -lGLU
TARGET = main
SRCS = main.cpp

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: clean
