#pragma once
#include <cstdint>
#include <memory>
class PS4Video {
public:
    PS4Video();
    ~PS4Video();
    void present(const uint32_t* bgra, int width, int height);
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
