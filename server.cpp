#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <string.h>

int main() {
    int serverfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverfd < 0) {
        std::perror("Cannot create socket");
        exit(0);
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(9090);
    socklen_t length = sizeof(server);

    if (bind(serverfd, (struct sockaddr *)&server, length) < 0) {
        std::perror("Cannot bind socket");
        exit(0);
    }

    struct sockaddr_in client;
    int clientfd, maxLength = 1500;
    char buffer[maxLength];
    while (true) {
        length = sizeof(client);
        std::cout << "Waiting for clients..." << std::endl;

        for (int i = 0; i < maxLength; i++) {
            buffer[i] = '\0';
        }

        int size = recvfrom(serverfd, (char *)buffer, maxLength,
                         MSG_WAITALL, ( struct sockaddr *) &client,
                         &length);
        if (size < 0) {
            perror("Receive error");
            exit(0);
        }

        std::cout << "Received \"" << buffer << "\" from client." << std::endl;

        char response[] = "What is up my dude";
        sendto(serverfd, response, strlen(response),
               0, (const struct sockaddr *) &client,
               length);

        close(clientfd);
    }

    close(serverfd);

    return 0;
}

