// UDP client program
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <sstream>

#define MAXLINE 1500

int main() {
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
    sendto(clientSocket, (char*) "send", sizeof("send"), 0, (const sockaddr*)&servaddr, sizeof(servaddr));

    int nextFrame = 0;

    while (true) {

        char message[MAXLINE];
        for (auto& j : message) j = '\0';

        int size = recvfrom(clientSocket, message, sizeof(message), 0, (sockaddr *)&servaddr, &length);
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
            sendto(clientSocket, ack.c_str(), sizeof(ack.c_str()), 0, (const sockaddr*)&servaddr, sizeof(servaddr));
        }
    }

    close(clientSocket);
    return 0;
}

