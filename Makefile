.PHONY: clean all

DEBUG_MODE = -O0 -g -ggdb3
RELEASE_MODE = -O2 
INCLUDES = -I./include
CXXFLAGS = -std=c++11 -Wall $(DEBUG_MODE) $(INCLUDES)
LDFLAGS = -pthread


all: server client

server: server.o
	g++ $(LDFLAGS) -o $@ $<

client: client.o
	g++ $(LDFLAGS) -o $@ $<

server.o: server.cpp
	g++ $(CXXFLAGS) -c $<

client.o: client.cpp
	g++ $(CXXFLAGS) -c $<

clean:
	rm -f server client *.o
