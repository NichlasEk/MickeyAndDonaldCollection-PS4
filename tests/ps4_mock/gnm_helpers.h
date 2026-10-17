#pragma once
#include <cstdint>
#define GNM_VIDEO_OUT_MAX_BUFFERS 4
enum GnmError { GNM_ERROR_OK=0, GNM_ERROR_INVALID_ARGS=-1 };
struct GnmVideoOut { int handle; uintptr_t flipqueue; uint32_t numbuffers,currentbuffer; uint64_t frame; };
