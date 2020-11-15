#pragma once

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include "utils.h"

#define MAX_CMD_LEN 255

class Client {
private:
    uint32_t id_;
    char server_ip_[32];
    int server_port_;
    int client_socket_;
    struct sockaddr_in server_addr_;
    struct sockaddr_in client_addr_;

    std::vector<UDP_Package*> sent_pkgs_;   // pkgs sent to server.
    std::vector<UDP_Package*> recv_pkgs_;   // pkgs received from server.

public:
    Client();
    ~Client();

    inline bool Valid () {
        return id_ != 0;
    }

    void Loop();
    void UserInterface();

    void ProcessList();
    void ProcessGet(std::string fname);
    void ProcessExit();

private:
    int InitSocket();
    std::vector<std::string> GetInputFromUser();
};

