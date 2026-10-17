#include "rom.h"
#include "video.h"
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <algorithm>
extern "C" void vk_ps4_log_open(const char*);

namespace {
FILE* report;
void note(const char* message) {
    std::printf("MDC: %s\n",message);
    sceKernelDebugOutText(0,message);
    if(report) { std::fprintf(report,"%s\n",message); std::fflush(report); }
}
struct Pad {
    int handle=-1;
    uint32_t previous=0, pressed=0;
    Pad() {
        int rc=sceUserServiceInitialize(nullptr);
        if(rc && uint32_t(rc)!=ORBIS_USER_SERVICE_ERROR_ALREADY_INITIALIZED) throw std::runtime_error("UserService init failed");
        int user=-1;
        if(sceUserServiceGetInitialUser(&user)) throw std::runtime_error("No initial PS4 user");
        scePadInit(); handle=scePadOpen(user,ORBIS_PAD_PORT_TYPE_STANDARD,0,nullptr);
        if(uint32_t(handle)==ORBIS_PAD_ERROR_ALREADY_OPENED) handle=scePadGetHandle(user,ORBIS_PAD_PORT_TYPE_STANDARD,0);
        if(handle<=0) throw std::runtime_error("Pad open failed");
    }
    uint16_t poll() {
        OrbisPadData state{};
        int rc=scePadReadState(handle,&state);
        if(rc<0 || !state.connected) { previous=pressed=0; return 0; }
        uint32_t b=state.buttons; pressed=b&~previous; previous=b;
        uint16_t md=0;
        if((b&ORBIS_PAD_BUTTON_UP)||state.leftStick.y<80) md|=MD_UP;
        if((b&ORBIS_PAD_BUTTON_DOWN)||state.leftStick.y>176) md|=MD_DOWN;
        if((b&ORBIS_PAD_BUTTON_LEFT)||state.leftStick.x<80) md|=MD_LEFT;
        if((b&ORBIS_PAD_BUTTON_RIGHT)||state.leftStick.x>176) md|=MD_RIGHT;
        if(b&ORBIS_PAD_BUTTON_SQUARE) md|=MD_A;
        if(b&ORBIS_PAD_BUTTON_CROSS) md|=MD_B;
        if(b&ORBIS_PAD_BUTTON_CIRCLE) md|=MD_C;
        if(b&ORBIS_PAD_BUTTON_OPTIONS) md|=MD_START;
        return md;
    }
};
const char* ids[]={"coi","qs","woi"};
std::unique_ptr<Session> open_game(int index,std::unique_ptr<GameModule>& module) {
    for(const char* root : {"/data/mickey-donald/roms","/app0/roms"}) {
        std::string path=std::string(root)+"/"+ids[index]+".bin";
        FILE* probe=std::fopen(path.c_str(),"rb");
        if(!probe) continue;
        std::fclose(probe);
        auto rom=load_rom_file(path.c_str());
        auto found=identify_rom(rom);
        if(std::strcmp(found->id(),ids[index])) throw std::runtime_error("ROM filename and CRC identify different games");
        auto session=std::make_unique<Session>();
        if(!session->open(found.get(),rom)) throw std::runtime_error("ROM could not start");
        Settings settings; session->apply_settings(settings);
        module=std::move(found); note(module->title()); return session;
    }
    return {};
}
}
int main() {
    report=std::fopen("/data/mickey-donald-ps4.log","w");
    note("Mickey & Donald PS4 0.01: native Vulkan bring-up; audio output not implemented");
    try {
        vk_ps4_log_open("/data/mickey-donald-vulkan.log");
        PS4Video video; note("Vulkan initialized");
        Pad pad;
        std::unique_ptr<GameModule> module;
        std::unique_ptr<Session> session;
        int selected=0;
        for(int i=0;i<3 && !session;++i) { selected=i; session=open_game(i,module); }
        if(!session) note("No supported ROM: put coi.bin / qs.bin / woi.bin under /data/mickey-donald/roms. Showing animated test pattern.");
        std::vector<uint32_t> test(400*224);
        uint64_t frames=0, start=sceKernelGetProcessTime();
        for(;;) {
            uint16_t buttons=pad.poll();
            if(pad.pressed&ORBIS_PAD_BUTTON_L1) {
                for(int i=1;i<=3;++i) {
                    int next=(selected+i)%3;
                    // Destroy the old session before replacing the module it references.
                    std::unique_ptr<GameModule> next_module;
                    auto candidate=open_game(next,next_module);
                    if(candidate) { session.reset(); module=std::move(next_module); session=std::move(candidate); selected=next; break; }
                }
            }
            if(session) {
                if(pad.pressed&ORBIS_PAD_BUTTON_R1) session->soft_reset();
                session->run_frame(buttons,0);
                int w,h; const uint32_t* pixels=session->frame(w,h); video.present(pixels,w,h);
            } else {
                for(int y=0;y<224;++y) for(int x=0;x<400;++x)
                    test[y*400+x]=0xff000000u|((x+frames)%256)<<16|((y*255/223)<<8)|((x/50)%2?255:32);
                video.present(test.data(),400,224);
            }
            ++frames;
            if(frames%300==0) {
                uint64_t now=sceKernelGetProcessTime(); char status[160];
                std::snprintf(status,sizeof(status),"frames=%llu mode=%s average_fps=%.2f",(unsigned long long)frames,session?module->id():"pattern",frames*1000000.0/(now-start)); note(status);
            }
            // Upper bound only; VideoOut FIFO presentation may already pace the loop.
            uint64_t target=start+uint64_t(frames*1000000.0/(session?session->md().frame_rate():60.0));
            uint64_t now=sceKernelGetProcessTime();
            if(target>now) usleep(static_cast<useconds_t>(std::min<uint64_t>(target-now,20000)));
        }
    } catch(const std::exception& e) { note(e.what()); if(report) std::fclose(report); return 1; }
}
