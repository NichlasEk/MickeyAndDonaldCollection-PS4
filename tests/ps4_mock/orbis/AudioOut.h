#pragma once
#include <cstdint>
enum OrbisAudioOutPort { ORBIS_AUDIO_OUT_PORT_TYPE_MAIN=0 };
constexpr int ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO=1;
constexpr uint32_t ORBIS_AUDIO_OUT_ERROR_ALREADY_INIT=0x80260001u;
int32_t sceAudioOutInit();
int32_t sceAudioOutOpen(int,OrbisAudioOutPort,int,uint32_t,uint32_t,uint32_t);
int32_t sceAudioOutOutput(int,const void*);
int32_t sceAudioOutClose(int);
int32_t sceAudioOutSetVolume(int,int,int*);
