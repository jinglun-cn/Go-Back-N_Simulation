server: server.o
	g++ -pthread -o server server.o

client: client.o
	g++ -o client client.o

server.o: server.cpp
	g++ -std=c++11 -pthread -c server.cpp

client.o: client.cpp
	g++ -std=c++11 -c client.cpp

clean:
	rm client.o server.o
