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

#define MAX_RECV_BUFFER_SIZE 2048
#define MAX_CMD_LEN 255
#define DELAY_MEAN_SECOND  2    // in second.
#define DELAY_DEV_SECOND   1    // in second.
#define DELAY_MAX_TIME     5    // in second.
#define WHATEVER_SEQ     0xbaba
#define SLEEPTIME_AFTER_RECV  4     // in second.

// for receiving pkgs from the server.
// extern void RecvThread();

class Client {
private:
    uint32_t id_;
    char server_ip_[32];
    int server_port_;
    int client_socket_;
    struct sockaddr_in server_addr_;
    struct sockaddr_in client_addr_;

    uint32_t start_seq_;
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

    // a wrapper for sending pkg to the server.
    void SendPackageWrapper(UDP_Package *pkg);

    // receive msg from the server.
    UDP_Package* ReceiverPKG();
    void ToSendACK(UDP_Package *recv_pkg);
    // eof_seq is the last sequence number, for checking the whether
    // the whole file has been received.
    void CheckSeqNumber(uint32_t eof_seq);
    void ProcessGotFile(std::string fname);

private:
    int InitSocket();
    std::vector<std::string> GetInputFromUser();
};

