#pragma once

#include <iostream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <list>
#include <thread>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <netdb.h>
#include <assert.h>
#include <dirent.h>
#include <mutex>
#include <random>

#define MAGIC_NUM  0xabcd1234

// use a spin lock and sleep 1000 microsecond, 0.001 second.
#define SPINLOCK_WAIT() usleep(1000);

typedef enum PACKET_TYPE {
    PK_MSG   = 0,   // only msg in the pkg.
    PK_ACK   = 1,   // only in C->S msg.
    PK_LIST  = 2,   // C->S: append NONE. S->C: append file fnames.
    PK_GET   = 3,   // only in C->S msg.
    PK_FILE  = 4,   // C->S: append fname. S->C: append file data.
    PK_EOF   = 5,     // end of file. only in S->C msg.
    PK_NOF   = 6,     // no such file. only in S->C msg.
    PK_UNKNOWN,
} pk_type;

// to get local ip address.
struct hostent *get_local_ip();

// for file related functions.
std::vector<std::string> get_files_from_path(const char *path = "./");
bool FileExist(const char *fname);

// for coding.
void PutFixed32(std::string* dst, uint32_t value);
uint32_t DecodeFixed32(const char* ptr);

// for timer.
uint64_t NowMicros();
uint64_t ElapsedMicros(uint64_t start_time);
// init normal distribution for simulate time delay. 
// mean is the mean of time in second.
void InitNormalDistribution(int mean, int stddev);
// simulate delay in microsecond.
uint64_t SimulateDelay();

// structure of package:
// |<------------ header ----------->|<------- data -------->|
// |-----|--------|--------|---------|-----------------------|
//    |      |         |        |                   |--> data[]
//    |      |         |        |--> 4 bytes, seq number of the package.
//    |      |         |--> 4 bytes, client identifer.
//    |      |--> 4 byte, package type.     
//    |--> 4 bytes, magic number to identify the package.
class UDP_Package {
  private:
    uint32_t magic_;
    uint32_t type_;
    uint32_t id_; // client identifer, it is addr_ in ClientHandler.
    uint32_t seq_;
    std::string data_;

    uint32_t sent_times_;   // how many times this pkg has sent to clients. 0 means has not sent yet.
    uint64_t sent_time_;    // to measure the time out, in microsecond (10^-6 second).

  public:
    UDP_Package() : magic_(MAGIC_NUM), type_(PK_UNKNOWN), id_(0), seq_(0), sent_times_(0), sent_time_(0) {};
    // build package from raw message.
    explicit UDP_Package(const std::string buffer);
    ~UDP_Package() {};

    inline bool Valid() {
        return (magic_ == (uint32_t)(MAGIC_NUM));
    }

    inline size_t HeaderSize() { return 4 * sizeof(uint32_t); }

    void SetHeader(pk_type p, uint32_t id, uint32_t seq);
    // get/set API.
    void SetType(pk_type p);
    pk_type GetType() const;
    void SetID(uint32_t id);
    uint32_t GetID() const;
    void SetSeqNum(uint32_t s);
    uint32_t GetSeqNum() const;
    void AppendData(const std::string d, const std::string prefix = "", const std::string suffix = "");
    std::string GetData() const;

    // for time out.
    void SetSentTime(uint64_t start = 0);
    // return true if current microsecond - sent_time_ >= micro.
    bool IsTimeOut(uint64_t micro);

    // pack file names to data feild.
    void PackFileNames(std::vector<std::string> fnames);
    // pack this structure and return data which is ready to send.
    std::string PackIt();
    // unpack from a raw buffer.
    void UnpackIt(const std::string buffer);

    // for debug.
    std::string PrintInfo();
};



