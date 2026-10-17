#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
struct PS4AudioStats {
    uint64_t submitted_blocks=0, nonzero_blocks=0, underrun_frames=0;
    size_t queued_frames=0;
};
class PS4Audio {
public:
    static constexpr size_t target_frames=3072; // 64 ms at 48 kHz
    PS4Audio();
    ~PS4Audio();
    PS4Audio(const PS4Audio&)=delete;
    PS4Audio& operator=(const PS4Audio&)=delete;
    void push(const int16_t* samples,size_t frames);
    void reset();
    PS4AudioStats stats();
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
