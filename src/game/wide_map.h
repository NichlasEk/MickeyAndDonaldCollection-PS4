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
    bool boundary_filled=true;
    int left=0,right=0;
};

// Reconstruct only pixels outside the horizontal world boundary. Reflect the
// already rendered scenery about that fixed boundary, not the moving viewport.
// As the camera scrolls, the seam moves naturally offscreen, one pixel at a time.
// Sprites are never copied; the extension is decorative, not playable space.
inline bool wide_map_boundary(const VDP& v,int camera,int lo,int hi,
                              uint8_t* a,uint8_t* b,uint8_t* sprites) {
    const int e=v.ext,w=v.active_width();
    // Validate all sources before modifying any line, including unusual camera
    // positions during transitions. The caller keeps its masks on failure.
    for(int x=-e;x<w+e;++x) {
        if(x>=0 && x<w)continue;
        int world=camera+x;
        if(world>=lo && world<hi)continue;
        int source=(world<lo ? 2*lo-1-world : 2*hi-1-world)-camera;
        if(source < -e || source >= w+e)return false;
    }
    for(int x=-e;x<w+e;++x) {
        if(x>=0 && x<w)continue;
        int world=camera+x;
        if(world>=lo && world<hi)continue;
        int source=(world<lo ? 2*lo-1-world : 2*hi-1-world)-camera;
        a[x+e]=a[source+e];b[x+e]=b[source+e];sprites[x+e]=0;
    }
    return true;
}
