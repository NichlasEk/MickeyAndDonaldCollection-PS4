// Exercise the actual consumer thread with a deterministic, gated AudioOut device.
#include "../platform/ps4/audio.h"
#include "../platform/ps4/audio_pacing.h"
#include <orbis/AudioOut.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <algorithm>
namespace {
std::mutex mutex;
std::condition_variable cv;
std::vector<std::array<int16_t,1024>> captured;
unsigned permits=0;
bool released=false, fail_output=false, closed=false, drained=false, fail_open=false;
void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
void wait_blocks(size_t n) {
    std::unique_lock<std::mutex> lock(mutex);
    require(cv.wait_for(lock,std::chrono::seconds(2),[&]{return captured.size()>=n;}),"consumer timed out");
}
void advance() { std::lock_guard<std::mutex> lock(mutex); ++permits; cv.notify_all(); }
void unblock() { std::lock_guard<std::mutex> lock(mutex); released=true; cv.notify_all(); }
std::array<int16_t,1024> block(size_t index) { std::lock_guard<std::mutex> lock(mutex); return captured.at(index); }
void reset_mock() {
    std::lock_guard<std::mutex> lock(mutex);
    captured.clear(); permits=0; released=fail_output=closed=drained=fail_open=false;
}
}
int32_t sceAudioOutInit() { return 0; }
int32_t sceAudioOutOpen(int user,OrbisAudioOutPort,int index,uint32_t frames,uint32_t rate,uint32_t format) {
    require(user==0xff && index==0 && frames==512 && rate==48000 && format==1,"wrong AudioOut format");
    return fail_open?-10:7;
}
int32_t sceAudioOutOutput(int port,const void* data) {
    require(port==7,"wrong port");
    std::unique_lock<std::mutex> lock(mutex);
    if(!data) { drained=true; return 0; }
    const auto* in=static_cast<const int16_t*>(data);
    std::array<int16_t,1024> copy;
    std::copy(in,in+1024,copy.begin()); captured.push_back(copy); cv.notify_all();
    cv.wait(lock,[]{return permits || released;});
    if(permits) --permits;
    return fail_output?-123:0;
}
int32_t sceAudioOutClose(int port) { require(port==7,"wrong close port"); closed=true; return 0; }
int32_t sceAudioOutSetVolume(int port,int mask,int* volume) {
    require(port==7 && mask==3 && volume[0]==32768 && volume[1]==32768,"stereo volume setup"); return 0;
}
int main() {
    try {
        // 60 Hz presentation with a 512-frame device clock. An exact refill
        // target used to duplicate frames merely due to block quantization.
        size_t queued=0, device_phase=0;
        auto produce=[&]() {
            bool refill=ps4_audio_refill(queued);
            int advanced=0;
            while(ps4_audio_advance(queued,advanced,refill)) { queued+=801; ++advanced; }
            require(queued<=8192,"pacing overflow");
            return advanced;
        };
        require(produce()==4,"startup did not prime");
        for(int tick=0;tick<600;++tick) {
            device_phase+=800;
            while(device_phase>=512) { require(queued>=512,"pacing underrun"); queued-=512; device_phase-=512; }
            require(produce()==1,"block quantization duplicated or skipped a game frame");
        }
        // Recover a real stall, and stop producing when the display is faster
        // than the device. The ring always stays bounded.
        queued=500; require(produce()==4,"stall did not catch up");
        queued=PS4Audio::target_frames+1024; require(produce()==0,"high queue did not throttle");
        reset_mock();
        {
            PS4Audio audio;
            // Always unblock the mock before AudioOut's destructor joins the thread.
            struct Release { ~Release(){unblock();} } release;
            wait_blocks(1);
            auto startup=block(0);
            require(std::all_of(startup.begin(),startup.end(),[](int16_t x){return x==0;}),"startup silence");
            std::vector<int16_t> samples(4096*2);
            for(size_t i=0;i<4096;++i) { samples[2*i]=int16_t(i+1); samples[2*i+1]=-int16_t(i+1); }
            audio.push(samples.data(),4096);
            for(size_t b=0;b<8;++b) {
                advance(); wait_blocks(b+2);
                auto actual=block(b+1);
                require(std::equal(actual.begin(),actual.end(),samples.begin()+b*1024),"stereo order / ring wrap corruption");
            }
            advance(); wait_blocks(10);
            require(audio.stats().underrun_frames==512,"underrun not counted");
            auto silent=block(9);
            require(std::all_of(silent.begin(),silent.end(),[](int16_t x){return x==0;}),"underrun not silenced");
            // Partial refill must not emit fragments until the queue is primed.
            audio.push(samples.data(),100);
            advance(); wait_blocks(11);
            require(audio.stats().queued_frames==100,"unprimed queue was consumed");
            audio.reset(); require(audio.stats().queued_frames==0,"reset did not discard stale samples");
            audio.push(samples.data(),4096);
            advance(); wait_blocks(12);
            auto reset_block=block(11);
            require(std::equal(reset_block.begin(),reset_block.end(),samples.begin()),"reset retained old samples");
            // Reject overflow without corrupting the ring or locking the consumer.
            bool overflow=false;
            try { audio.push(samples.data(),8193); } catch(const std::runtime_error&) { overflow=true; }
            require(overflow,"overflow accepted");
            require(audio.stats().queued_frames==3584,"overflow changed queue");
        }
        require(closed && drained,"shutdown did not drain and close");
        reset_mock();
        {
            PS4Audio audio;
            struct Release { ~Release(){unblock();} } release;
            wait_blocks(1);
            { std::lock_guard<std::mutex> lock(mutex); fail_output=true; }
            advance();
            bool detected=false;
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(std::chrono::steady_clock::now()<deadline && !detected) {
                try { (void)audio.stats(); } catch(const std::runtime_error&) { detected=true; }
            }
            require(detected,"worker error was not propagated");
        }
        require(closed && drained,"failed worker was not closed");
        reset_mock(); fail_open=true;
        bool rejected=false;
        try { PS4Audio audio; } catch(const std::runtime_error&) { rejected=true; }
        require(rejected && !closed,"failed open lifecycle");
        std::puts("PASS: stereo FIFO, underrun silence/reprime, reset, overflow, worker errors, shutdown and failed open");
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL %s\n",e.what()); return 1; }
}
