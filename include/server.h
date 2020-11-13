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
#include <map>
#include <queue>

#include "utils.h"

#define MAX_RECV_BUFFER_SIZE  2048
#define MAX_SUBMIT_QUEUE_SIZE 4
#define WINDOW_SIZE   MAX_SUBMIT_QUEUE_SIZE
#define SERVER_PORT 9090
#define FILE_CHUNK_SIZE 1024
#define LOST_ERROR_PERCENT 20   // [0,100]
#define OOS_ERROR_PERCENT 10    // [0,100]
// Time To Life (TTL) of a package to indicate time out issue.
#define PKG_TTL  10000000 // 10 second, 10^7 microsecond.

class ClientHandler;
class AtomicPKGQueue;
// uint32_t: sockaddr_in->sin_addr.s_addr to identify the client.
extern std::map<uint32_t, ClientHandler*> *global_clients;
// wait queue. pkgs from clients, have not sent yet.
extern AtomicPKGQueue *global_wait_queue;  
// submit queue. pkgs have sent to the client, but not received ACK yet.
extern AtomicPKGQueue *global_submit_queue;

// simulate something.
bool ShouldLost();  // lost of pkg.
bool ShouldOOS();   // out of sequence.

// check and process time out pkgs.
void CheckProcessTimeOut(int server_socket);
// a wrapper for sending pkg to clients.
void SendPackageWrapper(int server_socket, UDP_Package *pkg);

// INFO: create ClientHandler-slide socket.
// RETURN: return socket, -1 if failed.
int InitClientHandlerSocket();

// thread function.
void ReceiverThread(int server_socket);
void SenderThread(int server_socket);

class ClientHandler {
private:
    uint32_t addr_;   // sockaddr_in->sin_addr.s_addr to identify the client.
    uint32_t seq_;    // seq number to identify packges.
    struct sockaddr_in client_addr_;
    // all pkgs generated from this server-client connection.
    // stored in here for clean up.
    std::vector<UDP_Package*> all_pkgs_;  


public:
    ClientHandler() : addr_(0), seq_(0) {
        memset(&client_addr_, 0, sizeof(struct sockaddr_in));
    }
    explicit ClientHandler(struct sockaddr_in *in) : addr_(in->sin_addr.s_addr), seq_(0) {
        memcpy(&client_addr_, in, sizeof(struct sockaddr_in));
    }
    ~ClientHandler();

    inline struct sockaddr_in *GetClientAddr() {
        return &client_addr_;
    }

    inline uint32_t GetID() {
        return addr_;
    }

    // process functions process received msg and build pkg.
    // this built pkg will push to global wait queue for sending.
    // unpack msg and pass it to specified function.
    void ProcessRaw(std::string recv_msg);
    void ProcessPKG(UDP_Package *recv_pkg);
    // check ack msg and remove compeleted pkgs from global_submit_queue.
    void ProcessACK(UDP_Package* recv_pkg);
    // process ls command got from client.
    void ProcessList(UDP_Package* recv_pkg);
    // process get command got from client.
    void ProcessGet(UDP_Package* recv_pkg);
    // process msg command got from client.
    void ProcessMSG(UDP_Package* recv_pkg);

    // get all file frames based on the file name, then build pkgs and push to wait_queue_.
    void GetFileFrames();
    // build a pkg based on list files and push to wait_queue_.  
    void ListFiles();
};

// FIFO queue.
// thread safe.
class AtomicPKGQueue {
  private:
    // considering OOS select pkg from the queue, 
    // I decide to use vector instead of std::queue.
    std::vector<UDP_Package*> queue_;
    std::mutex mu_;
    size_t cap_;
public:
    AtomicPKGQueue() : cap_(0) {};
    explicit AtomicPKGQueue(size_t cap = 0) : cap_(cap) {};
    ~AtomicPKGQueue() {};

    inline bool IsFull() {
        return (cap_ == 0) ? false : (queue_.size() >= cap_);
    }

    inline bool IsEmpty() {
        return queue_.empty();
    }

    // get out of time packages.
    // thread safe.
    // DO NOT remove this OOT pkg from this queue.
    std::vector<UDP_Package*> GetOOTPKG();

    // push to the back of the queue.
    // check whether this queue is full before push.
    void PushBack(UDP_Package*);
    // pop one pkg from the front and remove it from the queue.
    UDP_Package* PopFront();
    // pop one pkg from the queue, based on out of sequence policy.
    // if not OOS, pop from front, otherwise pop the second one.
    UDP_Package* PopPKG();
    // remove specified pkg from this queue.
    UDP_Package* Remove(uint32_t id, uint32_t seq);
};


