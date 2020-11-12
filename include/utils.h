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
#include <netdb.h>
#include <assert.h>


#define MAGIC_NUM  0xabcd1234

typedef enum PACKET_TYPE {
    PK_MSG   = 0,
    PK_ACK   = 1,
    PK_LIST  = 2,
    PK_GET   = 3,
    PK_UNKNOWN,
} pk_type;

struct hostent *get_local_ip();
void PutFixed32(std::string* dst, uint32_t value);
uint32_t DecodeFixed32(const char* ptr);
uint64_t NowMicros();
uint64_t ElapsedMicros(uint64_t start_time);

// structure of package:
// |-----|--------|----------------|-----------------------|
//    |      |            |                   |--> data[]
//    |      |            |--> 4 bytes, seq number of the package.
//    |      |--> 4 byte, package type.     
//    |--> 4 bytes, magic number to identify the package.
class UPD_Package {
  private:
    uint32_t magic_;
    uint32_t type_;
    uint32_t seq_;
    std::string data_;

  public:
    UPD_Package() : magic_(MAGIC_NUM), type_(PK_UNKNOWN), seq_(0) {};
    // build package from raw message.
    explicit UPD_Package(const std::string buffer);
    ~UPD_Package() {};

    inline bool Valid() {
        return (magic_ == (uint32_t)(MAGIC_NUM));
    }

    // get/set API.
    void SetType(pk_type p);
    pk_type GetType();
    void SetSeqNum(uint32_t s);
    uint32_t GetSeqNum();
    void AppendData(const std::string d);
    std::string GetData();

    // pack this structure and return data which is ready to send.
    std::string PackIt();
    // unpack from a raw buffer.
    void UnpackIt(const std::string buffer);
};



