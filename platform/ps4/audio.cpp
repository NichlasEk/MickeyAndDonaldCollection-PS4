#include "audio.h"
#include <orbis/AudioOut.h>
#include <pthread.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

struct PS4Audio::Impl {
    static constexpr size_t block=512, capacity=8192;
    std::array<int16_t,capacity*2> ring{};
    size_t read=0, count=0;
    int port=-1, failure=0;
    pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
    pthread_t thread{};
    bool started=false, stop=false, primed=false;
    uint64_t submitted=0, nonzero=0, underrun=0;
    static void* entry(void* arg) { static_cast<Impl*>(arg)->run(); return nullptr; }
    void run() {
        // The consumer never allocates. Two blocks remain alive while firmware
        // consumes the previous submission. AudioOutOutput provides device pacing.
        alignas(64) std::array<std::array<int16_t,block*2>,2> buffers{};
        size_t next=0;
        for(;;) {
            auto& out=buffers[next];
            std::fill(out.begin(),out.end(),0);
            pthread_mutex_lock(&mutex);
            if(stop) { pthread_mutex_unlock(&mutex); break; }
            if(!primed && count>=PS4Audio::target_frames) primed=true;
            if(primed) {
                size_t available=std::min(count,block);
                for(size_t i=0;i<available;++i) {
                    out[i*2]=ring[((read+i)%capacity)*2];
                    out[i*2+1]=ring[((read+i)%capacity)*2+1];
                }
                read=(read+available)%capacity; count-=available;
                if(available<block) { underrun+=block-available; primed=false; }
            }
            pthread_mutex_unlock(&mutex);
            int result=sceAudioOutOutput(port,out.data());
            pthread_mutex_lock(&mutex);
            if(result<0) failure=result;
            else {
                ++submitted;
                if(std::any_of(out.begin(),out.end(),[](int16_t x){return x!=0;})) ++nonzero;
            }
            pthread_mutex_unlock(&mutex);
            if(result<0) break;
            next^=1;
        }
        // Drain before the buffers leave scope, even after stopping normally.
        int result=sceAudioOutOutput(port,nullptr);
        if(result<0) { pthread_mutex_lock(&mutex); failure=result; pthread_mutex_unlock(&mutex); }
    }
    void init() {
        int rc=sceAudioOutInit();
        if(rc<0 && uint32_t(rc)!=ORBIS_AUDIO_OUT_ERROR_ALREADY_INIT) throw std::runtime_error("AudioOut init failed: "+std::to_string(rc));
        port=sceAudioOutOpen(0xff,ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,0,block,48000,ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        if(port<=0) throw std::runtime_error("AudioOut open failed: "+std::to_string(port));
        int volumes[2]={32768,32768};
        rc=sceAudioOutSetVolume(port,3,volumes);
        if(rc<0) throw std::runtime_error("AudioOut volume setup failed: "+std::to_string(rc));
        rc=pthread_create(&thread,nullptr,entry,this);
        if(rc) throw std::runtime_error("Audio thread creation failed: "+std::to_string(rc));
        started=true;
    }
    ~Impl() {
        if(started) {
            pthread_mutex_lock(&mutex); stop=true; pthread_mutex_unlock(&mutex);
            pthread_join(thread,nullptr);
        }
        if(port>0) sceAudioOutClose(port);
        pthread_mutex_destroy(&mutex);
    }
};
PS4Audio::PS4Audio():p(new Impl) { p->init(); }
PS4Audio::~PS4Audio()=default;
void PS4Audio::push(const int16_t* samples,size_t frames) {
    if(frames && !samples) throw std::runtime_error("Null audio samples");
    pthread_mutex_lock(&p->mutex);
    if(p->failure<0 || frames>Impl::capacity-p->count) {
        int failure=p->failure;
        pthread_mutex_unlock(&p->mutex);
        throw std::runtime_error(failure<0?"AudioOut worker failed: "+std::to_string(failure):"Audio queue overflow");
    }
    for(size_t i=0;i<frames;++i) {
        size_t dst=((p->read+p->count+i)%Impl::capacity)*2;
        p->ring[dst]=samples[i*2]; p->ring[dst+1]=samples[i*2+1];
    }
    p->count+=frames;
    pthread_mutex_unlock(&p->mutex);
}
void PS4Audio::reset() {
    pthread_mutex_lock(&p->mutex);
    p->read=p->count=0; p->primed=false;
    pthread_mutex_unlock(&p->mutex);
}
PS4AudioStats PS4Audio::stats() {
    pthread_mutex_lock(&p->mutex);
    PS4AudioStats s{p->submitted,p->nonzero,p->underrun,p->count};
    int failure=p->failure;
    pthread_mutex_unlock(&p->mutex);
    if(failure<0) throw std::runtime_error("AudioOut worker failed: "+std::to_string(failure));
    return s;
}
