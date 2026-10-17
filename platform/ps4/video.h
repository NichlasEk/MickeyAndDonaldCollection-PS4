#pragma once
#include <cstdint>
#include <memory>
struct PS4VideoTiming { uint64_t work_us=0, flip_us=0; };
class PS4Video {
public:
    PS4Video();
    ~PS4Video();
    void present(const uint32_t* bgra, int width, int height);
    PS4VideoTiming timing() const;
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
