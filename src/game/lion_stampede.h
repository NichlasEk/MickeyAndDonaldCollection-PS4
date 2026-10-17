#pragma once
#include "../core/md.h"
#include <algorithm>
#include <cstring>

// Stampede has three 256-pixel canyon animation cels and one sky cel in a
// 1024-pixel nametable. Adjacent columns are other cels, not additional world.
// Extend only the canyon sides using reflected original wall artwork; repeat
// the sky cel. This is reconstructed scenery, not newly exposed level geometry.
namespace lion_stampede {
inline bool active(const MegaDrive& md) {
    unsigned cam=md.ram16(0xcd36);
    return md.ram16(0xccce)==4 && md.ram16(0xcd40)==0 &&
        cam>=0x9300 && cam<=0xfff8 && md.ram32(cam+4)==0x001b51b6 &&
        md.vdp.active_width()==256 && (md.vdp.reg[2]&0x38)==0x20 &&
        (md.vdp.reg[4]&7)==4 && (md.vdp.reg[16]&0x33)==3;
}
inline void line(MegaDrive& md,int y,uint8_t* a,uint8_t* b) {
    auto& v=md.vdp;int e=v.ext;
    for(int plane=0;plane<2;++plane) {
        auto* out=plane?b:a;
        unsigned hsbase=(v.reg[13]&63)<<10;
        int scroll=(v.vram[hsbase+plane*2]<<8)|v.vram[hsbase+plane*2+1];
        int py=(y+v.vsram[plane])&255;
        unsigned base=plane?(v.reg[4]&7)<<13:(v.reg[2]&0x38)<<10;
        for(int x=-e;x<256+e;++x) {
            if(x>=0 && x<256)continue;
            int source=plane?(x&255):(x<0?-x-1:511-x);
            int px=(source-scroll)&1023;
            unsigned dest=base+(py/8)*256+(px/8)*2;
            uint16_t entry=(v.vram[dest]<<8)|v.vram[dest+1];
            uint8_t row[8];v.pattern_row(entry,py&7,row);
            out[x+e]=row[px&7];
        }
    }
}
// Mode changes can occur after the frame-start decision (death/level entry).
// Centre the already rendered frame on the normal canvas without touching the
// original image or CPU-visible VDP registers. The next frame uses its new mode.
inline void finish(MegaDrive& md,int configured_ext) {
    auto& v=md.vdp;
    if(!configured_ext || !v.ext || (!active(md) && v.ext==configured_ext))return;
    int target=320+configured_ext*2,old=v.fb_width;
    if(old!=target)for(int y=0;y<v.fb_lines;++y) {
        if(old>target)std::memmove(v.fb[y],v.fb[y]+(old-target)/2,target*sizeof(uint16_t));
        else {
            int border=(target-old)/2;
            std::memmove(v.fb[y]+border,v.fb[y],old*sizeof(uint16_t));
            std::fill(v.fb[y],v.fb[y]+border,0);
            std::fill(v.fb[y]+border+old,v.fb[y]+target,0);
        }
    }
    v.fb_width=target;
    if(!active(md))v.ext=configured_ext;
}

}
