.PHONY: clean all

DEBUG_MODE = -O0 -g -ggdb3 #-DSTDERR_LEVEL_LOG
RELEASE_MODE = -O2 
INCLUDES = -I./include
CXXFLAGS = -std=c++11 -Wall $(DEBUG_MODE) $(INCLUDES)
LDFLAGS = -pthread


all: server client

server: server.o utils.o
	g++ $(LDFLAGS) -o $@ $?

client: client.o utils.o
	g++ $(LDFLAGS) -o $@ $?

server.o: src/server.cpp
	g++ $(CXXFLAGS) -c $<

client.o: src/client.cpp
	g++ $(CXXFLAGS) -c $<

utils.o: src/utils.cpp
	g++ $(CXXFLAGS) -c $<

clean:
	rm -f server client *.o
