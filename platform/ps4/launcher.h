#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
constexpr int kPS4GameCount=5;
constexpr int kLauncherWidth=800, kLauncherHeight=448;
struct PS4GameEntry {
    std::vector<uint32_t> preview; // 320 x 180, generated locally from the user ROM
    bool ready=false;
    bool saved=false;
    std::string source="MISSING";
    std::string path;
};
// Original bitmap UI, rendered through the existing native Vulkan presenter.
void ps4_draw_launcher(std::vector<uint32_t>& pixels, int selected, int active,
                       const std::array<PS4GameEntry,kPS4GameCount>& games, const std::string& message);
