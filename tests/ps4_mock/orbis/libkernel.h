#pragma once
#include <cstdint>
using OrbisKernelEqueue=void*;
using OrbisKernelUseconds=uint32_t;
struct OrbisKernelEvent { int unused; };
#define ORBIS_KERNEL_ERROR_ETIMEDOUT 0x8002003Cu
uint64_t sceKernelGetProcessTime();
int sceKernelWaitEqueue(OrbisKernelEqueue,OrbisKernelEvent*,int,int*,OrbisKernelUseconds*);
