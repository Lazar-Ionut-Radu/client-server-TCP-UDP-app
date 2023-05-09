# Compiler
CXX = g++
CXXFLAGS = -g -std=c++17 -Wall -Wextra

all: server subscriber

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp utils.cpp

subscriber: subscriber.cpp
	$(CXX) $(CXXFLAGS) -o subscriber subscriber.cpp utils.cpp

clean:
	rm -f server subscriber
