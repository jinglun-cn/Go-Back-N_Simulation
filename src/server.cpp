#include "server.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "mylog.h"
#include "utils.h"

std::map<uint32_t, ClientHandler *> *global_clients;
AtomicPKGQueue *global_wait_queue;
AtomicPKGQueue *global_submit_queue;

static int server_socket;    

static void sig_int_handler(int sig_num){
    clean_up();
    exit(0);
}

void clean_up() {
    close(server_socket);
    for (auto it = global_clients->begin(); it != global_clients->end(); ++it) {
        delete it->second;
    }
    LOG("Finish clearn up.\n");
}

// simulate something.
bool ShouldLost() {
    int x = rand() % 100;
    if (x < LOST_ERROR_PERCENT) {
        return true;  // should lost.
    }
    return false;
}

bool ShouldOOS() {
    int x = rand() % 100;
    if (x < OOS_ERROR_PERCENT) {
        return true;  // should out of sequence.
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
    memset((void *)&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    memcpy(&serverAddr.sin_addr, hent->h_addr_list[0], hent->h_length);

    // Bind socket to ip address and port.
    if (bind(server_socket, (const sockaddr *)&serverAddr, sizeof(serverAddr)) <
        0) {
        fprintf(stderr, "Could not bind to socket.\n");
        LOG("%s", strerror(errno));
        return -1;
    }

    // print server ip address and port for client to connect.
    printf("Server IP: %s\n", inet_ntoa(serverAddr.sin_addr));
    printf("Port: %d\n\n", SERVER_PORT);

    return server_socket;
}

// OOT pkgs do not need to be removed from the queue.
// The server also doesn't care about how many same pkgs has sent to clients.
void CheckProcessTimeOut(int server_socket) {
    std::vector<UDP_Package *> pkg_vc = global_submit_queue->GetOOTPKG();
    for (size_t i = 0; i < pkg_vc.size(); ++i) {
        if (pkg_vc.at(i) != NULL) {
            SendPackageWrapper(server_socket, pkg_vc.at(i));
        }
    }
}

void SendPackageWrapper(int server_socket, UDP_Package *pkg) {
    assert(pkg != NULL);
    // send to client.
    std::string data = pkg->PackIt();
    auto it = global_clients->find(pkg->GetID());
    if (it == global_clients->end()) {
        LOG("Not found client from map<id, clients>. ID: %d\n",
            (int)pkg->GetID());
        return;
    }
    if (ShouldLost()) {
        // do nothing.
        LOG("This pkg is lost.\n");
    } else {
        int ret = sendto(server_socket, data.c_str(), data.size(), 0,
                         (const sockaddr *)(it->second->GetClientAddr()),
                         sizeof(struct sockaddr_in));
        if (ret < 0) {
            LOG("sendto failed! error: %s\n", strerror(errno));
        }
    }
    // update send time.
    pkg->SetSentTime();
}

void ReceiverThread(int server_socket) {
    ssize_t recv_bytes = 0;
    socklen_t addr_len = 0;
    struct sockaddr_in client_addr;
    char buf[MAX_RECV_BUFFER_SIZE];
    while (true) {
        // init.
        recv_bytes = 0;
        addr_len = 0;
        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        memset(buf, 0, MAX_RECV_BUFFER_SIZE);

        // receive from clients.
        recv_bytes = recvfrom(server_socket, buf, MAX_RECV_BUFFER_SIZE, 0,
                              (sockaddr *)(&client_addr), &addr_len);
        if (recv_bytes < 0) {
            LOG("recvfrom failed! error: %s\n", strerror(errno));
            continue;
        }

        // build a package.
        // this pkg's life is this function. After process, it should be freed.
        UDP_Package pkg(std::string(buf, recv_bytes));
        if (!pkg.Valid()) {
            LOG("unpack message failed!\n");
            continue;
        }

        // if this is a new client connection, insert a new client handler into global client map.
        auto it = global_clients->find(pkg.GetID());
        if (it == global_clients->end()) {
            ClientHandler *client = new ClientHandler(&client_addr);
            assert(client != NULL);
            assert(pkg.GetID() == client->GetID());
            auto rst = global_clients->insert(std::pair<uint32_t, ClientHandler*> (client->GetID(), client));
            if (rst.second == false) {
                LOG("Insert new client to global client map failed!\n");
                delete client;
                continue;
            }
        }

        // process received pkg.
        it = global_clients->find(pkg.GetID());
        assert(it != global_clients->end());
        it->second->ProcessPKG(&pkg);
    }
}

void SenderThread(int server_socket) {
    while (true) {
        while (global_wait_queue->IsEmpty() && global_submit_queue->IsFull()) {
            // if wait queue is empty means: no pkgs need to send.
            // if submit queue is full means: slide windown is full.
            SPINLOCK_WAIT();
            // check time out pkgs.
            CheckProcessTimeOut(server_socket);
        }
        // get pkg from wait queue.
        // Out of sequence pkg will be determined in PopPKG function.
        UDP_Package *pkg = global_wait_queue->PopPKG();
        if (pkg == NULL) {
            // this queue is empty.
            continue;
        }
        SendPackageWrapper(server_socket, pkg);
        CheckProcessTimeOut(server_socket);
    }
}

ClientHandler::~ClientHandler() {
    for (size_t i = 0; i < all_pkgs_.size(); ++i) {
        delete all_pkgs_.at(i);
    }
    all_pkgs_.clear();
}

void ClientHandler::ProcessRaw(std::string recv_msg) {
    UDP_Package recv_pkg(recv_msg);
    if (!recv_pkg.Valid()) {
        LOG("unpack received data failed!\n");
        return;
    }
    ProcessPKG(&recv_pkg);
}

void ClientHandler::ProcessPKG(UDP_Package *recv_pkg) {
    switch (recv_pkg->GetType()) {
        case PK_MSG:
            this->ProcessMSG(recv_pkg);
            break;
        case PK_ACK:
            this->ProcessACK(recv_pkg);
            break;
        case PK_LIST:
            this->ProcessList(recv_pkg);
            break;
        case PK_GET:
            this->ProcessGet(recv_pkg);
            break;
        default:
            LOG("%d: invalid PACKET_TYPE\n", recv_pkg->GetType());
            break;
    }
}

void ClientHandler::ProcessACK(UDP_Package *recv_pkg) {
    assert(recv_pkg->GetType() == PK_ACK);
    // remove pkg from global_sub_queue.
    global_submit_queue->Remove(recv_pkg->GetID(), recv_pkg->GetSeqNum());
}

void ClientHandler::ProcessList(UDP_Package *recv_pkg) {
    assert(recv_pkg->GetType() == PK_LIST);
    // get file names from the dir.
    std::vector<std::string> fnames = get_files_from_path("./");
    // build a package.
    UDP_Package *pkg = new UDP_Package();
    pkg->SetHeader(PK_LIST, addr_, __sync_fetch_and_add(&seq_, 1));
    for (size_t i = 0; i < fnames.size(); ++i) {
        pkg->AppendData(fnames.at(i), "", "  ");
    }
    // push pkg.
    all_pkgs_.push_back(pkg);
    global_wait_queue->PushBack(pkg);
}

void ClientHandler::ProcessGet(UDP_Package *recv_pkg) {
    assert(recv_pkg->GetType() == PK_GET);
    std::string fname = recv_pkg->GetData();
    if (fname.size() > 128 || !FileExist(fname.c_str())) {
        LOG("%s: No such file.\n", fname.c_str());
        // build a package.
        UDP_Package *pkg = new UDP_Package();
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
                UDP_Package *pkg = new UDP_Package();
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

void ClientHandler::ProcessMSG(UDP_Package *recv_pkg) {
    assert(recv_pkg->GetType() == PK_MSG);
    in_addr add;
    add.s_addr = addr_;
    printf("receive msg from %s: %s\n", inet_ntoa(add),
           recv_pkg->GetData().c_str());
}

std::vector<UDP_Package *> AtomicPKGQueue::GetOOTPKG() {
    std::vector<UDP_Package *> ret;
    mu_.lock();
    for (size_t i = 0; i < queue_.size(); ++i) {
        if (queue_.at(i)->IsTimeOut(PKG_TTL)) {
            ret.push_back(queue_.at(i));
        }
    }
    mu_.unlock();
    return ret;
}

void AtomicPKGQueue::PushBack(UDP_Package *pkg) {
    mu_.lock();
    queue_.push_back(pkg);
    mu_.unlock();
}

UDP_Package *AtomicPKGQueue::PopFront() {
    UDP_Package *ret = NULL;
    mu_.lock();
    if (!queue_.empty()) {
        ret = queue_.at(0);
        queue_.erase(queue_.begin());
    }
    mu_.unlock();
    return ret;
}

UDP_Package *AtomicPKGQueue::PopPKG() {
    UDP_Package *ret = NULL;
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

UDP_Package *AtomicPKGQueue::Remove(uint32_t id, uint32_t seq) {
    UDP_Package *ret = NULL;
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
    signal(SIGINT,sig_int_handler);
    
    ///////////////////////////////////////////////////////////////////////////
    // STEP 1: init global variables.
    ///////////////////////////////////////////////////////////////////////////
    global_clients = new std::map<uint32_t, ClientHandler *>();
    global_wait_queue = new AtomicPKGQueue(0);
    global_submit_queue = new AtomicPKGQueue(WINDOW_SIZE);

    ///////////////////////////////////////////////////////////////////////////
    // STEP 2: create server socket.
    ///////////////////////////////////////////////////////////////////////////
    server_socket = InitServerSocket();
    if (server_socket < 0) {
        fprintf(stderr, "Init server socket failed!\n");
        exit(1);
    }

    ///////////////////////////////////////////////////////////////////////////
    // STEP 3: issue one thread for sending message, and another one for
    // receiving.
    ///////////////////////////////////////////////////////////////////////////
    std::thread sender(SenderThread, server_socket);
    std::thread receiver(ReceiverThread, server_socket);
    receiver.join();
    sender.join();

    return 0;
}
