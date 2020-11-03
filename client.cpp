// UDP client program 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <strings.h> 
#include <sys/socket.h> 
#include <sys/types.h>
#include <string.h>
#include <fcntl.h> 
#include <unistd.h>
#include <iostream>

#define PORT 9090 
#define MAXLINE 1500 

int main() {
    int sockfd;
    char buffer[MAXLINE]; 
    struct sockaddr_in servaddr;  
    int n;
    socklen_t len; 

    // Creating socket file descriptor 
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
        std::cout << "socket creation failed" << std::endl; 
        exit(0); 
    } 
     
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = inet_addr("128.186.120.181"); 
    
    std::cout << "\nenter a message to the echo server:" << std::endl; 
    std::cin.getline(buffer, MAXLINE);
     
    // send message to server
    sendto(sockfd, (const char*)buffer, strlen(buffer), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr)); 
    
    // receive server's response 
    for (int i = 0; i < MAXLINE; i++) {
        buffer[i] = '\0';
    }
 
    n = recvfrom(sockfd, (char*)buffer, MAXLINE, 0, (struct sockaddr*)&servaddr, &len); 
    std::cout << "Message from server:" << std::endl;
    std::cout << buffer << std::endl; 
    close(sockfd);
    return 0;
} 
