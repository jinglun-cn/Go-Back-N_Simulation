#include "client.h"

#include <assert.h>
#include <signal.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <istream>
#include <thread>

#include "mylog.h"

#define MAXLINE 1500

static Client *global_client = NULL;

static void sig_int_handler(int sig_num) {
    // TODO Use sig_num or remove it.

    delete global_client;
    exit(0);
}

Client::Client() {
    id_ = 0;
    client_socket_ = -1;

    memset(server_ip_, 0, 32);
    printf("Please enter the server IP: ");
    scanf("%s", server_ip_);
    printf("Please enter the server Port: ");
    std::cin >> server_port_;

    if (this->InitSocket() != 0) {
        LOG("New Client failed!\n");
    }
}

Client::~Client() {
    if (client_socket_ >= 0) {
        // TODO: send close pkg to the server.
        close(client_socket_);
    }
    for (auto pkg : recv_pkgs_) {
        delete pkg;
    }
}

int Client::InitSocket() {
    // Create a socket.
    client_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
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
    client_addr_.sin_port = htons(CLIENT_PORT);
    memcpy(&client_addr_.sin_addr, hent->h_addr_list[0], hent->h_length);
    id_ = client_addr_.sin_addr.s_addr;

    // Bind socket to ip address and port.
    if (bind(client_socket_, (const sockaddr *)&client_addr_,
             sizeof(client_addr_)) < 0) {
        id_ = 0;
        fprintf(stderr, "Could not bind to socket.\n");
        LOG("%s", strerror(errno));
        return -1;
    }

    // init server_addr_ for future sending.
    memset((void *)&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(server_port_);
    server_addr_.sin_addr.s_addr = inet_addr(server_ip_);

    // set timeout
    struct timeval timeout;
    timeout.tv_sec = SOCKET_TIMEOUT;
    timeout.tv_usec = 0;
    if (setsockopt (client_socket_, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                    sizeof(timeout)) < 0)
        LOG("setsockopt failed\n");

    if (setsockopt (client_socket_, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                    sizeof(timeout)) < 0)
        LOG("setsockopt failed\n");

    // send a hello world to the server for testing.
    // TEST:
//    UDP_Package pkg;
//    pkg.SetHeader(PK_MSG, id_, WHATEVER_SEQ);
//    pkg.AppendData("Hello World\n");
//    std::string data = pkg.PackIt();
//    int ret =
//            sendto(client_socket_, data.c_str(), data.size(), 0,
//                   (const sockaddr *)(&server_addr_), sizeof(struct sockaddr_in));
//    if (ret < 0) {
//        id_ = 0;
//        LOG("sendto failed! error: %s\n", strerror(errno));
//    }

    return 0;
}

void Client::SendPackageWrapper(UDP_Package *pkg) {
    assert(pkg != NULL);
    // send to the server.
    std::string data = pkg->PackIt();
    uint64_t start = NowMicros();
    // simulate delay in microsecond.
    uint64_t tt = SimulateDelay();
    if (tt > (DELAY_MAX_TIME * 1000000)) {
        tt = DELAY_MAX_TIME * 1000000;
    }
    printf("Simulate timed out delay %lu micro-second after send a package ...\n", (unsigned long)(tt));
    usleep(tt);
    int ret =
            sendto(client_socket_, data.c_str(), data.size(), 0,
                   (const sockaddr *)(&server_addr_), sizeof(struct sockaddr_in));
    if (ret < 0) {
        LOG("sendto failed! error: %s\n", strerror(errno));
    }
    // update send time.
    pkg->SetSentTime(start);
}

std::vector<std::string> Client::GetInputFromUser() {
    std::vector<std::string> cmds;
    char buf[MAX_CMD_LEN];
    memset(buf, 0, MAX_CMD_LEN);

    // Bug fix in first iteration.
    if (std::cin.peek() == '\n')
        std::cin.ignore(1);

    std::cin.getline(buf, MAX_CMD_LEN);

    char *token = strtok(buf, " ");
    while (token != NULL) {
        cmds.push_back(std::string(token));
        token = strtok(NULL, " ");
    }

    return cmds;
}

UDP_Package *Client::ReceiverPKG() {
    ssize_t recv_bytes = 0;
    socklen_t addr_len = 0;
    struct sockaddr_in conn_addr;
    char buf[MAX_RECV_BUFFER_SIZE];

    // init.
    recv_bytes = 0;
    addr_len = 0;
    memset(&conn_addr, 0, sizeof(struct sockaddr_in));
    memset(buf, 0, MAX_RECV_BUFFER_SIZE);

    // receive from clients.
    recv_bytes = recvfrom(client_socket_, buf, MAX_RECV_BUFFER_SIZE, 0,
                          (sockaddr *)(&conn_addr), &addr_len);
    if (recv_bytes < 0) {
        // LOG("recvfrom failed! error: %s\n", strerror(errno));
        return NULL;
    }

    printf("Sleep %d second after receive a package from the server...\n",
           SLEEPTIME_AFTER_RECV);
    sleep(SLEEPTIME_AFTER_RECV);

    // build a package.
    // this pkg's life is this function. After process, it should be freed.
    UDP_Package *pkg = new UDP_Package(std::string(buf, recv_bytes));

    LOG("%s\n", pkg->PrintInfo().c_str());

    if (!pkg->Valid()) {
        LOG("unpack message failed!\n");
        return NULL;
    }

    bool has_recv = false;
    for (size_t i = 0; i < recv_pkgs_.size(); ++i) {
        if (pkg->GetSeqNum() == recv_pkgs_.at(i)->GetSeqNum()) {
            has_recv = true;
            LOG("Has received!\n");
            break;
        }
    }
    // SendACK no matter has_recv or not
    ToSendACK(pkg);
    if (has_recv == true) {
        return NULL;
    } else {
        recv_pkgs_.push_back(pkg);
        return pkg;
    }
}

void Client::ToSendACK(UDP_Package *recv_pkg) {
    UDP_Package *pkg = new UDP_Package();
    pkg->SetHeader(PK_ACK, id_, recv_pkg->GetSeqNum());
    this->SendPackageWrapper(pkg);

    printf("Send a seq %d's ACK to the server.\n",
           (int)(recv_pkg->GetSeqNum()));
}

void Client::Loop() {
    while (true) {
        this->UserInterface();
        std::vector<std::string> cmds = this->GetInputFromUser();

        if (cmds.size() == 0) {
            continue;
        }

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
    UDP_Package *pkg = new UDP_Package();
    pkg->SetHeader(PK_LIST, id_, WHATEVER_SEQ);
    this->SendPackageWrapper(pkg);

    printf("Wait the response from the server...\n");

    UDP_Package *recv_pkg = NULL;
    while (true) {
        recv_pkg = ReceiverPKG();
        if (recv_pkg == NULL) {
            LOG("Error in ReceiverPKG function or this PKG has been received before.\n");
            continue;
        }
        break;
    }

    switch (recv_pkg->GetType()) {
        case PK_LIST:
            printf("\nServer: \n%s\n", recv_pkg->GetData().c_str());
            start_seq_ = recv_pkg->GetSeqNum();
            break;
        default:
            LOG("%d: invalid PACKET_TYPE after send list to the server.\n",
                recv_pkg->GetType());
            break;
    }
}

void Client::ProcessGet(std::string fname) {
    UDP_Package *pkg = new UDP_Package();
    pkg->SetHeader(PK_GET, id_, WHATEVER_SEQ);
    pkg->AppendData(fname);
    this->SendPackageWrapper(pkg);

    printf("Wait the response from the server...\n");

    while (true) {
        bool done = false;
        UDP_Package *recv_pkg = NULL;
        while (true) {
            recv_pkg = ReceiverPKG();
            if (recv_pkg == NULL) {
                LOG("Error in ReceiverPKG function or this PKG has been received before.\n");
                continue;
            }
            break;
        }

        switch (recv_pkg->GetType()) {
            case PK_FILE:
                LOG("receive a piece of file data, seq num: %d.\n",
                    (int)(recv_pkg->GetSeqNum()))
                break;
            case PK_EOF:
                CheckSeqNumber(recv_pkg->GetSeqNum());
                ProcessGotFile(fname);
                done = true;
                break;
            case PK_MSG:
            case PK_NOF:
                printf("%s: No such file exist in the server.\n",
                       pkg->GetData().c_str());
                done = true;
                break;
            default:
                LOG("%d: invalid PACKET_TYPE after send list to the server.\n",
                    recv_pkg->GetType());
                break;
        }

        if (done) {
            break;
        }
    }
}

void Client::CheckSeqNumber(uint32_t eof_seq) {
    std::vector<uint32_t> recv_seq;   // should received seq num.
    std::vector<uint32_t> wait_seq;  // not received seq num.

    // get all received pkgs' seq number.
    for (auto &pkg : recv_pkgs_) {
        recv_seq.push_back(pkg->GetSeqNum());
    }
    std::sort(recv_seq.begin(), recv_seq.end());

    // get the un-received pkgs' seq number.
    for (uint32_t i = start_seq_; i < eof_seq; ++i) {
        if (std::binary_search(recv_seq.begin(), recv_seq.end(), i) == false) {
            wait_seq.push_back(i);
        }
    }
    std::sort(wait_seq.begin(), wait_seq.end());

    // some pkgs are not received from the server, waiting...
    while (wait_seq.size() > 0) {
        UDP_Package *recv_pkg = NULL;
        while (true) {
            recv_pkg = ReceiverPKG();
            if (recv_pkg == NULL) {
                LOG("Error in ReceiverPKG function or this PKG has been received before.\n");
                continue;
            }
            break;
        }
        uint32_t seq = recv_pkg->GetSeqNum();
        if (std::binary_search(wait_seq.begin(), wait_seq.end(), seq) == true) {
            for (auto it = wait_seq.begin(); it != wait_seq.end(); ++it) {
                if ((*it) == seq) {
                    wait_seq.erase(it);
                    break;
                }
            }
        }
    }
    // got all packages, wait for a while to send remaining ACKs to close the connection
    uint64_t start_t = NowMicros();
    while (ElapsedMicros(start_t) < DELAY_CLOSE_TIME*1000000) {
        LOG("Cooling down to close the connection. Please wait...");
        ReceiverPKG();
    }
}

void Client::ProcessGotFile(std::string fname) {
    std::string data;
    std::vector<UDP_Package *> file_data;
    for (auto &pkg : recv_pkgs_) {
        if (pkg->GetType() == PK_FILE) {
            file_data.push_back(pkg);
        }
    }

    std::sort(file_data.begin(), file_data.end(),
              [](const UDP_Package *a, const UDP_Package *b) -> bool {
                  return (a->GetSeqNum() < b->GetSeqNum());
              });

    for (auto &pkg : file_data) {
        data.append(pkg->GetData());
    }

    // write data to local file.
    std::string new_fname = fname;
    new_fname.append(".xx.");
    new_fname.append(std::to_string(NowMicros()));
    std::ofstream myfile;
    myfile.open(new_fname);
    myfile << data;
    myfile.close();

    printf(
            "\nFile has retrived from the server, and written to the local, named: "
            "%s\n",
            new_fname.c_str());
}

void Client::ProcessExit() { delete this; }

void Client::UserInterface() {
    printf("\n---------------- MENU ------------------------\n");
    printf("list               list files in the server\n");
    printf("get <file name>    get the file from the server\n");
    printf("exit               exit this application\n");
    printf("----------------------------------------------\n\n");
}

int main(int argc, char const *argv[]) {
    InitNormalDistribution(DELAY_MEAN_SECOND, DELAY_DEV_SECOND);

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

