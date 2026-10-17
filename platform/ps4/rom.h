#pragma once
#include "app/session.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
inline std::unique_ptr<GameModule> identify_rom(const std::vector<uint8_t>& rom) {
    uint32_t crc = ~0u;
    for (uint8_t b : rom) {
        crc ^= b;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    crc = ~crc;
    using Factory = GameModule*(*)();
    for (Factory make : {create_coi_module, create_qs_module, create_woi_module, create_lion_module, create_aladdin_module}) {
        std::unique_ptr<GameModule> module(make());
        if (module->rom_size() == rom.size() && module->rom_crc32() == crc) return module;
    }
    throw std::runtime_error("Unsupported ROM version: exact raw USA/World revision required");
}
inline std::vector<uint8_t> load_rom_file(const char* path) {
    std::unique_ptr<FILE, int(*)(FILE*)> f(std::fopen(path, "rb"), std::fclose);
    if (!f) throw std::runtime_error(std::string("Cannot open ROM: ") + path);
    if (std::fseek(f.get(), 0, SEEK_END)) throw std::runtime_error("ROM seek failed");
    long size = std::ftell(f.get());
    if (size < 512 || size > 4*1024*1024 || (size & 1)) throw std::runtime_error("Expected raw Mega Drive ROM, 512 bytes to 4 MiB");
    std::rewind(f.get());
    std::vector<uint8_t> rom(static_cast<size_t>(size));
    if (std::fread(rom.data(), 1, rom.size(), f.get()) != rom.size()) throw std::runtime_error("ROM read failed");
    return rom;
}
