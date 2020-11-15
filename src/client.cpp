#include <signal.h>
#include <assert.h>
#include <istream> 
#include <iostream>

#include "client.h"
#include "mylog.h"

#define MAXLINE 1500

static Client *global_client = NULL;

static void sig_int_handler(int sig_num){
    delete global_client;
    exit(0);
}

int xxx() {
    int clientSocket;

    sockaddr_in servaddr = {};

    // Creating socket file descriptor
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cout << "socket creation failed" << std::endl;
        exit(0);
    }

    // Enter server ip and port.
    std::string serverIp;
    std::cout << "Enter the server IP: ";
    std::getline(std::cin, serverIp);

    int port;
    std::cout << "Enter port number: ";
    std::cin >> port;

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
    socklen_t length = sizeof(servaddr);

    std::cout << "Sending \"send\"" << std::endl;
    sendto(clientSocket, (char *)"send", sizeof("send"), 0,
           (const sockaddr *)&servaddr, sizeof(servaddr));

    int nextFrame = 0;

    while (true) {
        char message[MAXLINE];
        for (auto &j : message) j = '\0';

        int size = recvfrom(clientSocket, message, sizeof(message), 0,
                            (sockaddr *)&servaddr, &length);
        if (size < 0) {
            std::cout << "Error reading message." << std::endl;
            continue;
        }

        // TODO Implement F bit to exit recvfrom loop.
        std::string finalPacket = "final packet";
        if (std::string(message).find(finalPacket) != std::string::npos) {
            std::cout << "Final Flag" << std::endl;
            break;
        }

        // TODO actually verify seq order.
        // Break frame into seqNum and packet
        int seqNum = nextFrame;
        if (seqNum == nextFrame) {
            // Purposeful bug.
            //            if (seqNum == 13 && firstTime) {
            //                firstTime = false;
            //                continue;
            //            }

            nextFrame++;

            // Send ACK
            std::string ack = std::to_string(nextFrame);
            std::cout << "Sending ACK : " << ack << std::endl;
            sendto(clientSocket, ack.c_str(), sizeof(ack.c_str()), 0,
                   (const sockaddr *)&servaddr, sizeof(servaddr));
        }
    }

    close(clientSocket);
    return 0;
}

Client::Client() : id_(0), client_socket_(-1) {
    memset(server_ip_, 0, 32);

    memset(server_ip_, 0, 32);
    printf("Please enter the server IP:\n");
    scanf("%s", server_ip_);
    printf("Please enter the server Port:\n");
    scanf("%d", &server_port_);

    if (this->InitSocket() != 0) {
        LOG("New Client failed!\n");
    }
}

Client::~Client() {
    if (client_socket_ >= 0) {
        // TODO: send close pkg to the server.
        close(client_socket_);
    }
    for (auto pkg : sent_pkgs_) {
        delete pkg;
    }
    for (auto pkg : recv_pkgs_) {
        delete pkg;
    }
}

int Client::InitSocket() {
    // Create a socket.
    int client_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket_ < 0) {
        fprintf(stderr, "Could not create socket.\n");
        LOG("%s", strerror(errno));
        return -1;
    }

    // get the local ip.
    struct hostent *hent = get_local_ip();

    // init local client to establish socket.
    memset((void *)&client_addr_, 0, sizeof(client_addr_));
    client_addr_.sin_family = AF_INET;
    client_addr_.sin_port = htons(server_port_);
    memcpy(&client_addr_.sin_addr, hent->h_addr_list[0], hent->h_length);

    // Bind socket to ip address and port.
    if (bind(client_socket_, (const sockaddr *)&client_addr_, sizeof(client_addr_)) <
        0) {
        fprintf(stderr, "Could not bind to socket.\n");
        LOG("%s", strerror(errno));
        return -1;
    }

    id_ = client_addr_.sin_addr.s_addr;

    // the client socket has been established.

    // init server_addr_ for future sending.
    memset((void *)&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(server_port_);
    server_addr_.sin_addr.s_addr = inet_addr(server_ip_);

    return 0;
}

std::vector<std::string> Client::GetInputFromUser() {
    std::vector<std::string> cmds;
    char buf[MAX_CMD_LEN];
    memset(buf, 0, MAX_CMD_LEN);
    std::cin.getline(buf, MAX_CMD_LEN);
    
    char *token = strtok(buf, " ");
    while (token != NULL) {
        cmds.push_back(std::string(token));
        token = strtok(NULL, " ");
    }

    return cmds;
}

void Client::Loop() {
    while (true) {
        this->UserInterface();
        std::vector<std::string> cmds = this->GetInputFromUser();

        if (cmds.size() == 0) {
            continue;
        }

        // for (size_t i = 0; i < cmds.size(); ++i) {
        //     LOG("%zu: %s\n", i, cmds.at(i).c_str());
        // }

        if (strncmp(cmds.at(0).c_str(), "list", 4) == 0) {
            this->ProcessList();
        } else if (strncmp(cmds.at(0).c_str(), "get", 3) == 0) {
            if (cmds.size() < 2) {
                printf("ERROR: Invalid command.\n");
                continue;
            }
            this->ProcessGet(cmds.at(1));
        } else if (strncmp(cmds.at(0).c_str(), "exit", 4) == 0) {
            break;
        } else {
            printf("ERROR: Invalid command.\n");
        }
    }

    this->ProcessExit();
}

void Client::ProcessList() {

}

void Client::ProcessGet(std::string fname) {

}

void Client::ProcessExit() {
    delete this;
}


void Client::UserInterface() {
    printf("\n---------------- MENU ------------------------\n");
    printf("list               list files in the server\n");
    printf("get <file name>    get the file from the server\n");
    printf("exit               exit this application\n");
    printf("----------------------------------------------\n\n");
}

int main(int argc, char const *argv[]) {
    global_client = new Client();
    assert(global_client != NULL);

    if (!global_client->Valid()) {
        fprintf(stderr, "Failed to create a new Client\n");
        return 0;
    }

    signal(SIGINT, sig_int_handler);

    global_client->Loop();

    return 0;
}
