#include "rom.h"
#include "video.h"
#include "audio.h"
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <array>
extern "C" void vk_ps4_log_open(const char*);
extern "C" void vk_ps4_log_close();

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
        // Optional local test fixture; never included by the package allowlist.
        if(FILE* test=std::fopen("/app0/mdc-test-level.txt","r")) {
            int level=-1; int parsed=std::fscanf(test,"%d",&level); std::fclose(test);
            if(parsed!=1 || level<0 || size_t(level)>=found->levels().size() || !session->start_level(level,0))
                throw std::runtime_error("Requested test level could not start");
        }
        module=std::move(found); note(module->title()); return session;
    }
    return {};
}
}
int main() {
    report=std::fopen("/data/mickey-donald-ps4.log","w");
    note("Mickey & Donald PS4 0.02: native Vulkan + 48000 Hz stereo AudioOut");
    try {
        vk_ps4_log_open("/data/mickey-donald-vulkan.log");
        PS4Video video; note("Vulkan initialized");
        // Preserve startup diagnostics without synchronous filesystem writes per draw.
        FILE* trace=std::fopen("/app0/mdc-vulkan-trace.txt","r");
        if(trace) std::fclose(trace); else vk_ps4_log_close();
        Pad pad;
        PS4Audio audio; note("AudioOut initialized: 48000 Hz S16 stereo, 512-frame blocks");
        std::unique_ptr<GameModule> module;
        std::unique_ptr<Session> session;
        int selected=0;
        for(int i=0;i<3 && !session;++i) { selected=i; session=open_game(i,module); }
        if(!session) note("No supported ROM: put coi.bin / qs.bin / woi.bin under /data/mickey-donald/roms. Showing animated test pattern.");
        std::vector<uint32_t> test(400*224);
        std::vector<int16_t> samples; samples.reserve(2048);
        uint64_t frames=0, emulated=0, window=0, emulated_base=0, start=sceKernelGetProcessTime();
        std::array<uint64_t,300> work_samples{}, core_samples{}, render_samples{};
        uint64_t flip_total=0;
        for(;;) {
            uint16_t buttons=pad.poll();
            if(pad.pressed&ORBIS_PAD_BUTTON_L1) {
                audio.reset(); // Intentional silence while CRC checking/loading the next game.
                for(int i=1;i<=3;++i) {
                    int next=(selected+i)%3;
                    // Destroy the old session before replacing the module it references.
                    std::unique_ptr<GameModule> next_module;
                    auto candidate=open_game(next,next_module);
                    if(candidate) { session.reset(); module=std::move(next_module); session=std::move(candidate); selected=next; window=0; emulated_base=emulated; start=sceKernelGetProcessTime(); flip_total=0; break; }
                }
            }
            if(session) {
                uint64_t core_start=sceKernelGetProcessTime();
                if(pad.pressed&ORBIS_PAD_BUTTON_R1) { audio.reset(); session->soft_reset(); }
                // Audio is the emulation clock. If presentation only reaches 30 Hz,
                // run about two complete emulated frames per displayed frame. Never
                // slow down or stretch game audio to match the display bottleneck.
                for(int catchup=0; catchup<8 && audio.stats().queued_frames<PS4Audio::target_frames; ++catchup) {
                    session->run_frame(buttons,0); ++emulated;
                    samples.clear(); session->mix_audio(samples,1.0);
                    audio.push(samples.data(),samples.size()/2);
                }
                int w,h; const uint32_t* pixels=session->frame(w,h);
                uint64_t core_us=sceKernelGetProcessTime()-core_start;
                video.present(pixels,w,h);
                core_samples[window]=core_us; render_samples[window]=video.timing().work_us;
                work_samples[window]=core_us+video.timing().work_us;
                flip_total+=video.timing().flip_us;
            } else {
                for(int y=0;y<224;++y) for(int x=0;x<400;++x)
                    test[y*400+x]=0xff000000u|((x+frames)%256)<<16|((y*255/223)<<8)|((x/50)%2?255:32);
                video.present(test.data(),400,224);
            }
            ++frames;
            if(++window==300) {
                uint64_t now=sceKernelGetProcessTime(); char status[512];
                auto stats=audio.stats();
                auto sorted=work_samples; std::sort(sorted.begin(),sorted.end());
                std::sort(core_samples.begin(),core_samples.end());
                std::sort(render_samples.begin(),render_samples.end());
                std::snprintf(status,sizeof(status),"frames=%llu mode=%s display_fps=%.2f emulation_fps=%.2f audio_blocks=%llu nonzero=%llu underrun_frames=%llu queued=%zu work_p95_us=%llu work_max_us=%llu flip_avg_us=%llu core_p95_us=%llu render_p95_us=%llu",
                    (unsigned long long)frames,session?module->id():"pattern",window*1000000.0/(now-start),(emulated-emulated_base)*1000000.0/(now-start),
                    (unsigned long long)stats.submitted_blocks,(unsigned long long)stats.nonzero_blocks,(unsigned long long)stats.underrun_frames,stats.queued_frames,
                    (unsigned long long)sorted[284],(unsigned long long)sorted.back(),(unsigned long long)(flip_total/300),
                    (unsigned long long)core_samples[284],(unsigned long long)render_samples[284]); note(status);
                flip_total=0; window=0; emulated_base=emulated; start=now;
            }
            // Without a game there is no audio producer to pace emulation.
            if(!session) usleep(1000);
        }
    } catch(const std::exception& e) { note(e.what()); if(report) std::fclose(report); return 1; }
}
