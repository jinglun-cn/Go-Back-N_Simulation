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

#define MAXLINE 1500
#define PORT 9090

void receiver(char** data, int numFrames);
void sender(char** data, int numFrames);
void timer(int *i);

std::list<int> outstandingFrames;
std::vector<int> badTimers;
int serverSocket;
sockaddr_in serverAddr;
sockaddr_in clientAddr;
socklen_t sizeOfClient;

int senderWindow = 4;

int main() {

    // Create a socket.
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        std::cout << "Could not create socket." << std::endl;
        return -1;
    }

    serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind socket to ip address and port.
    if (bind(serverSocket, (const sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cout << "Could not bind to socket." << std::endl;
        return -1;
    }

    clientAddr = {};
    sizeOfClient = sizeof(clientAddr);

    // Wait for "send" message.
    while (true) {
        char buffer[MAXLINE];
        for (char& i : buffer) i = '\0';

        // Wait for message.
        int size = recvfrom(serverSocket, buffer, 1500, 0, (sockaddr *)&clientAddr, &sizeOfClient);
        if (size == -1) {
            std::cout << "Could not read packet." << std::endl;
        } else if (strcmp(buffer, "send") != 0) {
            std::cout << "Waiting for send command..." << std::endl;
        } else break;
    }

    std::ifstream file("testfile.txt");
    char c;
    std::stringstream ss;
    while (!file.eof()) {
        file.get(c);
        ss << c;
    }
    file.close();

    char* data;
    std::string s = ss.str();
    data = new char[s.length()];
    for (int i = 0; i < s.length(); i++) {
        data[i] = s.at(i);
    }

    int numFrames = (s.length() / MAXLINE) + 1;
    char **fileBuffer = (char **) new char[numFrames][MAXLINE];
    for (int i = 0; i < numFrames; i++) {
        fileBuffer[i] = new char[MAXLINE];
        for (int j = 0; j < MAXLINE; j++) {
            fileBuffer[i][j] = '\0';
        }
    }

    for (int i = 0; i < numFrames; i++) {
        fileBuffer[i] = new char[MAXLINE];
        for (int j = 0; j < MAXLINE; j++) {
            if (MAXLINE*i + j == s.length()) break;
            fileBuffer[i][j] = data[MAXLINE*i + j];
        }
    }

    // Wait for "send", then send data.
    std::thread receive(receiver, fileBuffer, numFrames);
    std::thread send(sender, fileBuffer, numFrames);
    receive.join();
    send.join();

    // Close socket.
    close(serverSocket);

    return 0;
}

void receiver(char** data, int numFrames) {
    // Send command received.

    char buffer[MAXLINE];
    while (true) {
        for (char& i : buffer) i = '\0';

        // Wait for message.
        int size = recvfrom(serverSocket, buffer, MAXLINE, 0, (sockaddr *)&clientAddr, &sizeOfClient);
        if (size == -1) {
            std::cout << "Could not read packet." << std::endl;
            continue;
        }

        // Sleep - travel time.
        sleep(1);

        int ackReceived = 0;
        for (char& i : buffer) {
            if (i == '\0') break;
            ackReceived = (10 * ackReceived) + ((int) i) - 48; // '0' = 48, '1' = 49...
        }

        // Valid ACK
        if (outstandingFrames.front() == ackReceived - 1) {
            outstandingFrames.pop_front();

            std::cout << (int) ackReceived * 100 / numFrames << "% complete..." << std::endl;

            if (ackReceived == numFrames - 1)
                break;
        } else {
            // ignore
        }
    }
    std::cout << "All frames sent." << std::endl;
}

void sender(char** data, int numFrames) {

    for (int i = 0; i <= numFrames; ) {
        if (i == numFrames && outstandingFrames.empty()) {
            i++;
            continue;
        } else if (i == numFrames) {
            continue;
        }

        if (outstandingFrames.empty()) {
            outstandingFrames.push_back(i);
            outstandingFrames.pop_back();
        }

        if (outstandingFrames.size() > senderWindow - 1) {
            continue;
        }

        // sleep - travel time
        usleep(500000);

        int currentI = i;
        if (currentI != numFrames - 1)
            outstandingFrames.push_back(i);

        std::cout << "Sending: " << data[currentI] << std::endl;

        // send each message.
        sendto(serverSocket, (const char*) data[currentI], MAXLINE, 0, (const sockaddr *)&clientAddr, sizeOfClient);

        std::cout << "\nCURRENT WINDOW : ";
        for (auto& j : outstandingFrames)
            std::cout << j << "  ";
        std::cout << "\n" << std::endl;

        // start timer.
        std::thread time(timer, &i);
        time.detach();

        // did a timeout occur? no - increment i, yes - let new frame i continue
        if (currentI == i) {
            i++;
        }
    }
}

void timer(int *i) {
    int outstandingFrame = *i;
    sleep(5);

    for (auto& j : badTimers) {
        if (j == outstandingFrame) {
            std::cout << "bad timer! " << j << std::endl;
            return;
        }
    }

    for (auto& j : outstandingFrames) {
        if (j == outstandingFrame) {
            std::cout << "Found outstanding frame" << std::endl;
            for (int k = 1; k < senderWindow; k++) {
                badTimers.push_back(k + outstandingFrame);
            }
            std::cout << outstandingFrame << std::endl;
            outstandingFrame = outstandingFrames.front();
            while (!outstandingFrames.empty())
                outstandingFrames.pop_back();

            *i = outstandingFrame;
            break;
        }
    }
}

char* frameToPacket(char* frame) {
    char* packet = new char[MAXLINE];

    return packet;
}

char* packetToFrame(char* packet) {
    char* frame = new char[MAXLINE];

    return frame;
}