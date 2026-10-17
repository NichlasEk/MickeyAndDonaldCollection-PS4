#pragma once
#include <cstdint>
struct OrbisVideoOutFlipStatus { int64_t flipArg; int32_t currentBuffer; };
int sceVideoOutSubmitFlip(int,int,uint32_t,int64_t);
int sceVideoOutGetFlipStatus(int,OrbisVideoOutFlipStatus*);
