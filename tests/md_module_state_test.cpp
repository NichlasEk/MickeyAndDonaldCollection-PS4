#include "core/md.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
static void require(bool ok,const char* reason) {if(!ok) throw std::runtime_error(reason);}
int main() {
    try {
        auto md=std::make_unique<MegaDrive>();
        std::vector<uint8_t> rom(512,0);
        require(md->load_rom(rom),"test ROM");md->power_on();
        md->ram[123]=45;md->module_state[2]=1234;
        md->module_data={1,2,3,4,5,6,7};
        auto saved=md->save_state();
        auto restored=std::make_unique<MegaDrive>();restored->load_rom(rom);restored->power_on();
        require(restored->load_state(saved),"v3 reload");
        require(restored->module_data==md->module_data,"presentation data lost");
        require(restored->save_state()==saved,"v3 round trip changed machine");
        // v2 has the same fixed machine prefix and no presentation trailer.
        auto legacy=saved;legacy.resize(legacy.size()-4-md->module_data.size());
        uint32_t version=2;std::memcpy(legacy.data()+4,&version,4);
        require(restored->load_state(legacy),"legacy v2 rejected");
        require(restored->module_data.empty(),"legacy load retained stale presentation");
        require(restored->ram[123]==45 && restored->module_state[2]==1234,"legacy machine changed");
        auto before=restored->save_state();
        uint32_t written_version=0;std::memcpy(&written_version,before.data()+4,4);
        require(written_version==2 && before==legacy,"ordinary states must retain v2 format");
        auto truncated=saved;truncated.pop_back();
        require(!restored->load_state(truncated),"truncated trailer accepted");
        require(restored->save_state()==before,"failed load changed running machine");
        auto oversized=saved;uint32_t size=uint32_t(MegaDrive::kMaxModuleData+1);
        std::memcpy(oversized.data()+legacy.size(),&size,4);
        require(!restored->load_state(oversized),"oversized trailer accepted");
        require(restored->save_state()==before,"oversized load changed machine");
        auto unknown=saved;version=99;std::memcpy(unknown.data()+4,&version,4);
        require(!restored->load_state(unknown),"unknown format accepted");
        restored->module_data={9};restored->power_on();
        require(restored->module_data.empty(),"reset retained old presentation");
        restored->module_data.resize(MegaDrive::kMaxModuleData+1);
        bool rejected=false;
        try {restored->save_state();} catch(const std::length_error&) {rejected=true;}
        require(rejected,"oversized save accepted");
        std::cout<<"PASS presentation state, v2 compatibility, bounds and atomic rejection\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
