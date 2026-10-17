#include "state_store.h"
#include <cstdio>
#include <unistd.h>
#include <stdexcept>
#include <sys/stat.h>
namespace {
constexpr size_t limit=4*1024*1024;
uint32_t checksum(const std::vector<uint8_t>& data) {
    uint32_t h=2166136261u;
    for(auto b:data) { h^=b; h*=16777619u; }
    return h;
}
void fail() { throw std::runtime_error("Save file missing, damaged or incompatible"); }
}
bool ps4_state_exists(const std::string& path) {
    struct stat s{}; return stat(path.c_str(),&s)==0 && S_ISREG(s.st_mode);
}
void ps4_write_state(const std::string& path,const std::vector<uint8_t>& data) {
    if(data.empty() || data.size()>limit) fail();
    const uint32_t header[]={0x5343444d,1,uint32_t(data.size()),checksum(data)};
    std::string temp=path+".tmp";
    FILE* f=std::fopen(temp.c_str(),"wb");
    if(!f) throw std::runtime_error("Cannot create save file");
    bool ok=std::fwrite(header,1,sizeof(header),f)==sizeof(header) &&
            std::fwrite(data.data(),1,data.size(),f)==data.size();
    if(ok) ok=std::fflush(f)==0;
    if(ok) ok=fsync(fileno(f))==0;
    if(std::fclose(f)!=0) ok=false;
    if(!ok || std::rename(temp.c_str(),path.c_str())!=0) {
        std::remove(temp.c_str()); throw std::runtime_error("Save write failed; previous slot retained");
    }
}
std::vector<uint8_t> ps4_read_state(const std::string& path) {
    FILE* f=std::fopen(path.c_str(),"rb"); if(!f) fail();
    uint32_t h[4]{};
    if(std::fread(h,1,sizeof(h),f)!=sizeof(h) || h[0]!=0x5343444d || h[1]!=1 || !h[2] || h[2]>limit) {
        std::fclose(f); fail();
    }
    std::vector<uint8_t> data(h[2]);
    bool ok=std::fread(data.data(),1,data.size(),f)==data.size();
    if(std::fgetc(f)!=EOF || std::ferror(f)) ok=false;
    std::fclose(f);
    if(!ok || checksum(data)!=h[3]) fail();
    return data;
}
