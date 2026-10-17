#include "rom.h"
#include "video.h"
#include "audio.h"
#include "audio_pacing.h"
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
    for(const char* root : {"/data/mickey-donald/roms",
                            "/mnt/usb0/mickey-donald/roms","/mnt/usb1/mickey-donald/roms",
                            "/mnt/usb2/mickey-donald/roms","/mnt/usb3/mickey-donald/roms",
                            "/app0/roms"}) {
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
        module=std::move(found); note(("ROM loaded: "+path).c_str()); note(module->title()); return session;
    }
    return {};
}
void benchmark(int frames) {
    PS4Video video(true);
    vk_ps4_log_close();
    note("BENCHMARK: private 1280x720 target, complete GPU fence per frame; AudioOut and display excluded");
    for(int game=0;game<3;++game) {
        std::unique_ptr<GameModule> module;
        auto session=open_game(game,module);
        if(!session) continue;
        std::vector<uint64_t> times; times.reserve(frames);
        std::vector<int16_t> audio; audio.reserve(2048);
        uint64_t total=0, core=0, render=0, gpu_wait=0;
        for(int frame=-120;frame<frames;++frame) {
            uint64_t begin=sceKernelGetProcessTime();
            session->run_frame(0,0); audio.clear(); session->mix_audio(audio,1.0);
            int w,h; const auto* pixels=session->frame(w,h);
            uint64_t core_us=sceKernelGetProcessTime()-begin;
            video.present(pixels,w,h);
            uint64_t elapsed=sceKernelGetProcessTime()-begin;
            if(frame>=0) {
                times.push_back(elapsed); total+=elapsed; core+=core_us;
                render+=video.timing().work_us; gpu_wait+=video.timing().gpu_wait_us;
            }
        }
        std::sort(times.begin(),times.end()); char result[512];
        std::snprintf(result,sizeof(result),"BENCH game=%s frames=%d fps=%.2f p50_us=%llu p95_us=%llu p99_us=%llu max_us=%llu core_avg_us=%llu render_avg_us=%llu gpu_wait_avg_us=%llu budget_12500us_p99=%s",
            module->id(),frames,frames*1000000.0/total,
            (unsigned long long)times[(frames-1)*50/100],(unsigned long long)times[(frames-1)*95/100],
            (unsigned long long)times[(frames-1)*99/100],(unsigned long long)times.back(),
            (unsigned long long)(core/frames),(unsigned long long)(render/frames),
            (unsigned long long)(gpu_wait/frames),times[(frames-1)*99/100]<=12500?"PASS":"FAIL");
        note(result);
    }
}

}
int main() {
    report=std::fopen("/data/mickey-donald-ps4.log","w");
    note("Mickey & Donald PS4 0.03: native Vulkan + 48000 Hz stereo AudioOut");
    try {
        vk_ps4_log_open("/data/mickey-donald-vulkan.log");
        if(FILE* fixture=std::fopen("/app0/mdc-benchmark-frames.txt","r")) {
            int frames=0; int parsed=std::fscanf(fixture,"%d",&frames); std::fclose(fixture);
            if(parsed!=1 || frames<300 || frames>10000) throw std::runtime_error("Benchmark requires 300..10000 frames");
            benchmark(frames); note("BENCH complete; normal display and AudioOut acceptance remain separate");
            if(report) std::fclose(report); return 0;
        }
        PS4Video video; note("Vulkan initialized");
        // Preserve startup diagnostics without synchronous filesystem writes per draw.
        FILE* trace=std::fopen("/app0/mdc-vulkan-trace.txt","r");
        if(trace) std::fclose(trace); else vk_ps4_log_close();
        // Exercise the real draw path before starting the audio clock. Drivers
        // may compile/cache the first draw lazily; don't drain the audio FIFO
        // while that one-time work is still pending.
        const uint32_t black[4]={0xff000000u,0xff000000u,0xff000000u,0xff000000u};
        video.present(black,2,2);
        video.present(black,2,2); // Completes the first draw's GPU fence.
        Pad pad;
        PS4Audio audio; note("AudioOut initialized: 48000 Hz S16 stereo, 512-frame blocks");
        std::unique_ptr<GameModule> module;
        std::unique_ptr<Session> session;
        int selected=0;
        for(int i=0;i<3 && !session;++i) { selected=i; session=open_game(i,module); }
        if(!session) note("No supported ROM: put coi.bin / qs.bin / woi.bin under /data/mickey-donald/roms or /mnt/usb0/mickey-donald/roms. Showing animated test pattern.");
        std::vector<uint32_t> test(400*224);
        std::vector<int16_t> samples; samples.reserve(2048);
        uint64_t frames=0, emulated=0, window=0, emulated_base=0, start=sceKernelGetProcessTime();
        std::array<uint64_t,300> work_samples{}, core_samples{}, render_samples{};
        uint64_t flip_total=0, repeated=0, catchup_frames=0;
        std::array<uint64_t,5> stages{};
        for(;;) {
            uint16_t buttons=pad.poll();
            if(pad.pressed&ORBIS_PAD_BUTTON_L1) {
                audio.reset(); // Intentional silence while CRC checking/loading the next game.
                for(int i=1;i<=3;++i) {
                    int next=(selected+i)%3;
                    // Destroy the old session before replacing the module it references.
                    std::unique_ptr<GameModule> next_module;
                    auto candidate=open_game(next,next_module);
                    if(candidate) { session.reset(); module=std::move(next_module); session=std::move(candidate); selected=next; window=0; emulated_base=emulated; start=sceKernelGetProcessTime(); flip_total=0; repeated=catchup_frames=0; stages.fill(0); break; }
                }
            }
            uint64_t core_us=0;
            if(session) {
                uint64_t core_start=sceKernelGetProcessTime();
                if(pad.pressed&ORBIS_PAD_BUTTON_R1) { audio.reset(); session->soft_reset(); }
                // Keep one game frame per display tick during steady 60 Hz
                // output. AudioOut consumes 512-frame blocks, while one game
                // frame produces about 801 samples: chasing an exact queue
                // target every tick can alternate duplicate and catch-up frames.
                // A two-block deadband absorbs that quantization. Larger queue
                // deficits still trigger bounded catch-up after real stalls.
                size_t queued=audio.stats().queued_frames;
                bool refill=ps4_audio_refill(queued);
                int advanced=0;
                while(ps4_audio_advance(queued,advanced,refill)) {
                    session->run_frame(buttons,0); ++emulated; ++advanced;
                    samples.clear(); session->mix_audio(samples,1.0);
                    audio.push(samples.data(),samples.size()/2);
                    queued=audio.stats().queued_frames;
                }
                if(advanced==0) ++repeated;
                if(advanced>1) catchup_frames+=advanced-1;
                int w,h; const uint32_t* pixels=session->frame(w,h);
                core_us=sceKernelGetProcessTime()-core_start;
                video.present(pixels,w,h);
            } else {
                for(int y=0;y<224;++y) for(int x=0;x<400;++x)
                    test[y*400+x]=0xff000000u|((x+frames)%256)<<16|((y*255/223)<<8)|((x/50)%2?255:32);
                video.present(test.data(),400,224);
            }
            core_samples[window]=core_us; render_samples[window]=video.timing().work_us;
            work_samples[window]=core_us+video.timing().work_us;
            auto t=video.timing();
            flip_total+=t.flip_us;
            stages[0]+=t.gpu_wait_us; stages[1]+=t.upload_us; stages[2]+=t.acquire_us;
            stages[3]+=t.record_us; stages[4]+=t.submit_us;
            ++frames;
            if(++window==300) {
                uint64_t now=sceKernelGetProcessTime(); char status[768];
                auto stats=audio.stats();
                auto sorted=work_samples; std::sort(sorted.begin(),sorted.end());
                std::sort(core_samples.begin(),core_samples.end());
                std::sort(render_samples.begin(),render_samples.end());
                std::snprintf(status,sizeof(status),"frames=%llu mode=%s display_fps=%.2f emulation_fps=%.2f audio_blocks=%llu nonzero=%llu underrun_frames=%llu queued=%zu work_p95_us=%llu work_max_us=%llu work_p99_us=%llu flip_avg_us=%llu core_p95_us=%llu render_p95_us=%llu gpu_wait_avg_us=%llu upload_avg_us=%llu acquire_avg_us=%llu record_avg_us=%llu submit_avg_us=%llu repeated=%llu catchup_frames=%llu",
                    (unsigned long long)frames,session?module->id():"pattern",window*1000000.0/(now-start),(emulated-emulated_base)*1000000.0/(now-start),
                    (unsigned long long)stats.submitted_blocks,(unsigned long long)stats.nonzero_blocks,(unsigned long long)stats.underrun_frames,stats.queued_frames,
                    (unsigned long long)sorted[284],(unsigned long long)sorted.back(),(unsigned long long)sorted[296],(unsigned long long)(flip_total/300),
                    (unsigned long long)core_samples[284],(unsigned long long)render_samples[284],
                    (unsigned long long)(stages[0]/300),(unsigned long long)(stages[1]/300),
                    (unsigned long long)(stages[2]/300),(unsigned long long)(stages[3]/300),
                    (unsigned long long)(stages[4]/300),(unsigned long long)repeated,(unsigned long long)catchup_frames); note(status);
                flip_total=0; repeated=catchup_frames=0; stages.fill(0); window=0; emulated_base=emulated; start=now;
            }
            // Without a game there is no audio producer to pace emulation.
            if(!session) usleep(1000);
        }
    } catch(const std::exception& e) { note(e.what()); if(report) std::fclose(report); return 1; }
}
