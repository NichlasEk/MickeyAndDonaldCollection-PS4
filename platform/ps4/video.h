#pragma once
#include <cstdint>
#include <memory>
struct PS4VideoTiming { uint64_t work_us=0, flip_us=0, gpu_wait_us=0, upload_us=0, acquire_us=0, record_us=0, submit_us=0; };
class PS4Video {
public:
    explicit PS4Video(bool benchmark=false);
    ~PS4Video();
    void present(const uint32_t* bgra, int width, int height);
    PS4VideoTiming timing() const;
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
