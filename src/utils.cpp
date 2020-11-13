#include "utils.h"
#include "mylog.h"

static std::mt19937 random_generator;
static std::normal_distribution<double> *normal_dist = NULL;

uint64_t NowMicros() {
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	return (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
}

uint64_t ElapsedMicros(uint64_t start_time) {
	uint64_t now = NowMicros();
	return now - start_time;
}

void InitNormalDistribution(int mean, int stddev) {
    if (normal_dist == NULL) {
        normal_dist = new std::normal_distribution<double>(mean, stddev);
    }
    assert(normal_dist != NULL);
}

uint64_t SimulateDelay() {
    assert(normal_dist != NULL);
    double x = (*normal_dist)(random_generator);
    return (uint32_t)(x * 1000000);
}

struct hostent *get_local_ip() {
    char hname[128];
    struct hostent *hent;
    gethostname(hname, sizeof(hname));
    hent = gethostbyname(hname);
    if (hent == NULL || ((sizeof(hent->h_addr_list) / sizeof(hent->h_addr_list[0])) < 1)) {
        fprintf(stderr, "%s-%d: get local ip failed!\n", __FILE__, __LINE__);
        return NULL;
    }
    return hent;
}

std::vector<std::string> get_files_from_path(const char* path) {
    DIR *dir;
    struct dirent *ent;
    std::vector<std::string> fnames;
    if ((dir = opendir (path)) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            // printf ("%s\n", ent->d_name);
            fnames.push_back(std::string(ent->d_name));
        }
        closedir (dir);
    } else {
        /* could not open directory */
        perror ("");
        LOG("%s", strerror(errno));
    }
    return fnames;
}

bool FileExist(const char *fname) {
    struct stat buffer;   
    return (stat(fname, &buffer) == 0); 
}

void PutFixed32(std::string* dst, uint32_t value) {
    dst->append(const_cast<const char*>(reinterpret_cast<char*>(&value)),
                sizeof(value));
}

uint32_t DecodeFixed32(const char* ptr) {
    uint32_t result;
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
}

UPD_Package::UPD_Package(const std::string buffer)
    : magic_(0), type_(PK_UNKNOWN), id_(0), seq_(0), sent_time_(0) {
    if (buffer.size() < this->HeaderSize()) {
        LOG("buffer is too short to be a packet.\n");
        return;
    }
    UnpackIt(buffer);
};

void UPD_Package::SetHeader(pk_type p, uint32_t id, uint32_t seq) {
    this->SetType(p);
    this->SetID(id);
    this->SetSeqNum(seq);
}

void UPD_Package::SetType(pk_type p) {
    assert(p < PK_UNKNOWN);
    type_ = (uint32_t)p;
}

pk_type UPD_Package::GetType() {
    return (pk_type)type_;
}

void UPD_Package::SetID(uint32_t id) {
    id_ = id;
}

uint32_t UPD_Package::GetID() {
    return id_;
}

void UPD_Package::SetSeqNum(uint32_t s) {
    seq_ = s;
}

uint32_t UPD_Package::GetSeqNum() {
    return seq_;
}

void UPD_Package::AppendData(const std::string d, const std::string prefix, const std::string suffix) {
    data_.append(prefix);
    data_.append(d);
    data_.append(suffix);
}

void UPD_Package::PackFileNames(std::vector<std::string> fnames) {
    for (auto& fname : fnames) {
        this->AppendData(fname, "", "  ");
    }
}

std::string UPD_Package::GetData() {
    return data_;
}

void UPD_Package::SetSentTime() {
    sent_time_ = NowMicros();
}

bool UPD_Package::IsTimeOut(uint64_t micro) {
    uint64_t now = NowMicros();
    if (now - sent_time_ >= micro) {
        return true;
    } else {
        return false;
    }
}

std::string UPD_Package::PackIt() {
    std::string buffer;

    // append magic number.
    PutFixed32(&buffer, magic_);
    // append type.
    PutFixed32(&buffer, type_);
    // append id.
    PutFixed32(&buffer, id_);
    // append seq number.
    PutFixed32(&buffer, seq_);
    // append data.
    buffer.append(data_);

    return buffer;
}

void UPD_Package::UnpackIt(const std::string buffer) {
    const char *ptr = buffer.c_str();

    // check magic number is correct.
    uint32_t magic_ = DecodeFixed32(ptr);
    if (magic_ != (uint32_t)(MAGIC_NUM)) {
        LOG("magic number: %ld, expected number %ld\n", (unsigned long)magic_, (unsigned long)MAGIC_NUM);
    } else {
        // get type.
        ptr += sizeof(uint32_t);
        type_ = DecodeFixed32(ptr);

        // get id.
        ptr += sizeof(uint32_t);
        id_ = DecodeFixed32(ptr);

        // get seq number.
        ptr += sizeof(uint32_t);
        seq_ = DecodeFixed32(ptr);

        // get data_.
        ptr += sizeof(uint32_t);
        data_.clear();
        if (buffer.size() > this->HeaderSize()) {
            data_.append(ptr, buffer.size() - this->HeaderSize()); 
        }
    }
}