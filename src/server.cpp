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
#include <random>

#include "utils.h"
#include "server.h"
#include "mylog.h"

std::map<uint32_t, ClientHandler*> *clients;
AtomicPKGQueue *global_wait_queue;  
AtomicPKGQueue *global_submit_queue;

// simulate something.
bool ShouldLost() {
    int x = rand() % 100;
    if (x < LOST_ERROR_PERCENT) {
        return true; // should lost.
    }
    return false;
}

bool ShouldOOS() {
    int x = rand() % 100;
    if (x < OOS_ERROR_PERCENT) {
        return true; // should out of sequence.
    }
    return false;
}

int InitServerSocket() {
    struct sockaddr_in serverAddr;
    // Create a socket.
    int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        fprintf(stderr, "Could not create socket.\n");
        LOG("%s", strerror(errno));
        return -1;
    }

    // get the local ip.
    struct hostent *hent = get_local_ip();

    // init serverAddr to establish socket.
    memset((void*)&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    memcpy(&serverAddr.sin_addr, hent->h_addr_list[0], hent->h_length);

    // Bind socket to ip address and port.
    if (bind(server_socket, (const sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        fprintf(stderr, "Could not bind to socket.\n");
        LOG("%s", strerror(errno));
        return -1;
    }

    // print server ip address and port for client to connect.
    printf("Server IP: %s\n", inet_ntoa(serverAddr.sin_addr));
    printf("Port: %d\n\n", SERVER_PORT);

    return server_socket;
}

void ReceiverThread(int server_socket) {

}

void SenderThread(int server_socket) {
    while (true) {
        while (global_wait_queue->IsEmpty() && !global_submit_queue->IsFull()) {
            SPINLOCK_WAIT();
        }
        // get pkg from wait queue.
        UPD_Package *pkg = global_wait_queue->PopPKG();
        if (pkg == NULL) {
            // this queue is empty.
            continue;
        }
        // send to client.
        std::string data = pkg->PackIt();
        auto it = clients->find(pkg->GetID());
        if (it == clients->end()) {
            LOG("Not found client from map<id, clients>. ID: %d\n", (int)pkg->GetID());
            continue;
        }
        sendto(server_socket, data.c_str(), data.size(), 0, 
                (const sockaddr *)(it->second->GetClientAddr()), sizeof(struct sockaddr_in));
    }
}


ClientHandler::~ClientHandler() {
    for (size_t i = 0; i < all_pkgs_.size(); ++i) {
        delete all_pkgs_.at(i);
    }
    all_pkgs_.clear();
}

void ClientHandler::ProcessRaw(std::string recv_msg) {
    UPD_Package recv_pkg(recv_msg);
    if (!recv_pkg.Valid()) {
        LOG("unpack received data failed!\n");
        return ;
    }
    switch (recv_pkg.GetType())     {
        case PK_MSG:
            this->ProcessMSG(&recv_pkg);
            break;
        case PK_ACK:
            this->ProcessACK(&recv_pkg);
            break;
        case PK_LIST:
            this->ProcessList(&recv_pkg);
            break;
        case PK_GET:
            this->ProcessGet(&recv_pkg);
            break;
        default:
            LOG("%d: invalid PACKET_TYPE\n", recv_pkg.GetType());
            break;
    }
}

void ClientHandler::ProcessACK(UPD_Package* recv_pkg) {
    assert(recv_pkg->GetType() == PK_ACK);
    // remove pkg from global_sub_queue.
    global_submit_queue->Remove(recv_pkg->GetID(), recv_pkg->GetSeqNum());
}

void ClientHandler::ProcessList(UPD_Package* recv_pkg) {
    assert(recv_pkg->GetType() == PK_LIST);
    // get file names from the dir.
    std::vector<std::string> fnames = get_files_from_path("./");
    // build a package.
    UPD_Package *pkg = new UPD_Package();
    pkg->SetHeader(PK_LIST, addr_, __sync_fetch_and_add(&seq_, 1));
    for (size_t i = 0; i < fnames.size(); ++i) {
        pkg->AppendData(fnames.at(i), "", "  ");
    }
    // push pkg.
    all_pkgs_.push_back(pkg);
    global_wait_queue->PushBack(pkg);
}

void ClientHandler::ProcessGet(UPD_Package* recv_pkg) {
    assert(recv_pkg->GetType() == PK_GET);
    std::string fname = recv_pkg->GetData();
    if (fname.size() > 128 || !FileExist(fname.c_str())) {
        LOG("%s: No such file.\n", fname.c_str());
        // build a package.
        UPD_Package *pkg = new UPD_Package();
        pkg->SetHeader(PK_NOF, addr_, __sync_fetch_and_add(&seq_, 1));
        // push pkg.
        all_pkgs_.push_back(pkg);
        global_wait_queue->PushBack(pkg);
    } else {
        std::ifstream fin(fname.c_str(), std::ifstream::in);
        if (fin.is_open()) {
            char buf[FILE_CHUNK_SIZE];
            while (!fin.eof()) {
                fin.read(buf, FILE_CHUNK_SIZE);
                // build a package.
                UPD_Package *pkg = new UPD_Package();
                pkg->SetHeader(PK_FILE, addr_, __sync_fetch_and_add(&seq_, 1));
                pkg->AppendData(std::string(buf, fin.gcount()));
                // push pkg.
                all_pkgs_.push_back(pkg);
                global_wait_queue->PushBack(pkg);
            }
        } else {
            LOG("%s: Open failed: %s\n", fname.c_str(), strerror(errno));
        }
    }
}

void ClientHandler::ProcessMSG(UPD_Package* recv_pkg) {
    assert(recv_pkg->GetType() == PK_MSG);
    in_addr add;
    add.s_addr = addr_;
    printf("receive msg from %s: %s\n", inet_ntoa(add), recv_pkg->GetData().c_str());
}


void AtomicPKGQueue::PushBack(UPD_Package* pkg) {
    mu_.lock();
    queue_.push_back(pkg);
    mu_.unlock();
}

UPD_Package* AtomicPKGQueue::PopFront() {
    UPD_Package *ret = NULL;
    mu_.lock();
    if (!queue_.empty()) {
        ret = queue_.at(0);
        queue_.erase(queue_.begin());
    }
    mu_.unlock();
    return ret;
}

UPD_Package* AtomicPKGQueue::PopPKG() {
    UPD_Package *ret = NULL;
    mu_.lock();
    if (!queue_.empty()) {
        auto it = queue_.begin();
        if (queue_.size() > 1 && ShouldOOS()) {
            ++it;
        }
        ret = *it;
        queue_.erase(it);
    }
    mu_.unlock();
    return ret;
}

UPD_Package* AtomicPKGQueue::Remove(uint32_t id, uint32_t seq) {
    UPD_Package *ret = NULL;
    mu_.lock();
    auto it = queue_.begin();
    for (; it != queue_.end(); ++it) {
        if ((*it)->GetID() == id && (*it)->GetSeqNum() == seq) {
            ret = *it;
            queue_.erase(it);
            break;
        }
    }
    mu_.unlock();
    return ret;
}

int main(int argc, char const *argv[]) {
    ///////////////////////////////////////////////////////////////////////////
    // STEP 0: init global variables.
    ///////////////////////////////////////////////////////////////////////////
    clients = new std::map<uint32_t, ClientHandler*>();
    global_wait_queue = new AtomicPKGQueue(0);
    global_submit_queue = new AtomicPKGQueue(WINDOW_SIZE);

    ///////////////////////////////////////////////////////////////////////////
    // STEP 1: create server socket.
    ///////////////////////////////////////////////////////////////////////////
    int server_socket = InitServerSocket();
    if (server_socket < 0) {
        fprintf(stderr, "Init server socket failed!\n");
        exit(1);
    }

    ///////////////////////////////////////////////////////////////////////////
    // STEP 2: issue one thread for sending message, and another one for receiving.
    ///////////////////////////////////////////////////////////////////////////
    std::thread sender(SenderThread, server_socket);
    std::thread receiver(ReceiverThread, server_socket);
    receiver.join();
    sender.join();

    ///////////////////////////////////////////////////////////////////////////
    // STEP 3: clean up.
    ///////////////////////////////////////////////////////////////////////////
    close(server_socket);
    for (auto it = clients->begin(); it != clients->end(); ++it) {
        delete it->second;
    }

    return 0;
}

