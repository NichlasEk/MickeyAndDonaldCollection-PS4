#include "rom.h"
#include "state_store.h"
#include <chrono>
#include <cstdlib>
#include <algorithm>
int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) { std::fprintf(stderr, "Usage: mdc_smoke RAW_ROM [STATE_PATH]\n"); return 2; }
    try {
        auto rom = load_rom_file(argv[1]);
        auto game = identify_rom(rom);
        Session session;
        if (!session.open(game.get(), rom)) throw std::runtime_error("ROM rejected");
        Settings settings;
        session.apply_settings(settings);
        const auto begin = std::chrono::steady_clock::now();
        uint64_t hash = 14695981039346656037ull;
        size_t samples = 0, audible = 0;
        int w = 0, h = 0;
        for (int frame = 0; frame < 600; ++frame) {
            session.run_frame(0, 0);
            const uint32_t* pixels = session.frame(w, h);
            if (w <= 0 || w > VDP::kMaxWidth || h <= 0 || h > VDP::kMaxLines) throw std::runtime_error("Invalid framebuffer");
            for (int i = 0; i < w*h; ++i) { hash ^= pixels[i]; hash *= 1099511628211ull; }
            std::vector<int16_t> audio;
            session.mix_audio(audio, 1.0);
            samples += audio.size();
            audible += std::count_if(audio.begin(), audio.end(), [](int16_t x) { return x != 0; });
        }
        const auto saved = session.save_state();
        session.run_frame(MD_RIGHT, 0);
        const auto expected = session.md().save_state();
        if (!session.load_state(saved)) throw std::runtime_error("Save-state reload failed");
        session.run_frame(MD_RIGHT, 0);
        if (session.md().save_state() != expected) throw std::runtime_error("State replay diverged");
        if(argc==3) {
            ps4_write_state(argv[2],saved);
            Session restored;
            if(!restored.open(game.get(),rom)) throw std::runtime_error("Fresh session failed");
            restored.apply_settings(settings);
            if(!restored.load_state(ps4_read_state(argv[2]))) throw std::runtime_error("Disk state reload failed");
            restored.run_frame(MD_RIGHT,0);
            if(restored.md().save_state()!=expected) throw std::runtime_error("Fresh-session disk replay diverged");
        }
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        if (!audible) throw std::runtime_error("No nonzero audio in 600 frames");
        std::printf("PASS game=%s frames=600 size=%dx%d hash=%016llx samples=%zu nonzero=%zu seconds=%.3f state_replay=match (HOST ONLY)\n", game->id(), w,h,static_cast<unsigned long long>(hash),samples,audible,elapsed);
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL %s\n", e.what()); return 1; }
}
