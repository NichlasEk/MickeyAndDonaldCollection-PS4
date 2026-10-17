#pragma once
#include "audio.h"

// The AudioOut block (512 samples) is smaller than a game frame (~801).
// Preserve one emulated frame per display tick within a two-block deadband.
inline bool ps4_audio_refill(size_t queued) {
    return queued < PS4Audio::target_frames - 1024;
}
inline bool ps4_audio_advance(size_t queued, int advanced, bool refill) {
    return advanced < 8 && (refill ? queued < PS4Audio::target_frames :
                          advanced == 0 && queued < PS4Audio::target_frames + 1024);
}
