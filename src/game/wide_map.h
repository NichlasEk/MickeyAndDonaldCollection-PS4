#pragma once
#include "../core/md.h"
// Resolve the VDP's wrapping scroll position near the game's world camera.
// The game may have advanced its camera before uploading the next scroll table.
// Using only the RAM camera would put extension pixels a few pixels out of phase.
inline int wide_map_x(const VDP& v,int line,int bank,int camera) {
    int off;
    switch(v.reg[11]&3) {
      case 0:off=0;break;case 1:off=(line&7)*4;break;
      case 2:off=(line&~7)*4;break;default:off=line*4;break;
    }
    unsigned addr=(((v.reg[13]&63)<<10)+off+bank*2)&65535;
    int scroll=(v.vram[addr]<<8)|v.vram[(addr+1)&65535];
    return camera+((-scroll-camera+256)&511)-256;
}
inline int wide_map_y(const VDP& v,int bank,int camera) {
    return camera+((int(v.vsram[bank])-camera+128)&255)-128;
}
// Capture masks from the frame being rendered, not the next camera position
// that may already have been prepared by the time post_frame is called.
struct WideMapView {
    MegaDrive* md=nullptr;
    bool gameplay=false;
    bool menu=false;
    int left=0,right=0;
};
