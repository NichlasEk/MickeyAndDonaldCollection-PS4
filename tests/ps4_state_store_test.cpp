#include "../platform/ps4/state_store.h"
#include <cstdio>
#include <stdexcept>
#include <unistd.h>
#include <sys/stat.h>
void check(bool b) { if(!b) throw std::runtime_error("state store check failed"); }
template<class F> void rejects(F f) { bool failed=false; try { f(); } catch(const std::exception&) { failed=true; } check(failed); }
int main() {
    char dir[]="/tmp/mdc-state-XXXXXX"; check(mkdtemp(dir));
    std::string path=std::string(dir)+"/coi.state";
    std::vector<uint8_t> a={1,2,3,4}, b={5,6,7};
    check(!ps4_state_exists(path)); rejects([&]{ps4_read_state(path);});
    ps4_write_state(path,a); check(ps4_read_state(path)==a);
    // A failed temporary-file creation must preserve the existing slot.
    check(mkdir((path+".tmp").c_str(),0700)==0);
    rejects([&]{ps4_write_state(path,b);});check(ps4_read_state(path)==a);
    rmdir((path+".tmp").c_str()); ps4_write_state(path,b);check(ps4_read_state(path)==b);
    FILE* f=fopen(path.c_str(),"r+b");check(f);fseek(f,16,SEEK_SET);fputc(99,f);fclose(f);
    rejects([&]{ps4_read_state(path);});
    check(truncate(path.c_str(),8)==0);rejects([&]{ps4_read_state(path);});
    ps4_write_state(path,a);f=fopen(path.c_str(),"ab");fputc(0,f);fclose(f);rejects([&]{ps4_read_state(path);});
    remove(path.c_str());rmdir(dir);puts("PASS state disk roundtrip, replacement, corruption, truncation, failed-write preservation");
}
