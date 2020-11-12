#pragma once

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <list>
#include <thread>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>

#define MAX_LISTEN_QUEUE 20
#define MAXLINE 1500
#define SERVER_PORT 9090


// INFO: create server-slide socket.
// RETURN: return socket, -1 if failed.
int InitServerSocket();

// INFO: listen connection from clients.
// RETURN: NULL.
void ListenLoop(int server_socket);

void receiver(char** data, int numFrames);
void sender(char** data, int numFrames);
void timer(int *i);

