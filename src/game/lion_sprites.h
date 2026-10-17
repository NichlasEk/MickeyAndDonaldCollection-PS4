#pragma once
#include "../core/md.h"
#include <algorithm>
#include <cstring>

// Presentation-only copy of Lion's metasprites. Observe the original sprite
// builder; never expand its clipping bounds or add emulated CPU/DMA work.
namespace lion_sprites {
constexpr unsigned kCount=256, kHeader=16;
struct Sprite { uint32_t source; int16_t x,y; uint16_t attr,shape; };
static_assert(sizeof(Sprite)==12,"stable sprite cache layout");
constexpr unsigned kBank=kCount*sizeof(Sprite), kSize=kHeader+2*kBank;
constexpr uint32_t kMagic=0x3153504c; // LPS1
inline uint32_t word(const MegaDrive& m,unsigned offset) {
    uint32_t value=0;std::memcpy(&value,m.module_data.data()+offset,4);return value;
}
inline void word(MegaDrive& m,unsigned offset,uint32_t value) {
    std::memcpy(m.module_data.data()+offset,&value,4);
}
inline void ensure(MegaDrive& m) {
    if(m.module_data.size()!=kSize || word(m,0)!=kMagic) {
        m.module_data.assign(kSize,0);word(m,0,kMagic);
    }
}
inline bool valid(const MegaDrive& m,unsigned addr,unsigned size) {
    return addr<m.rom.size() && size<=m.rom.size()-addr;
}
inline uint16_t read16(const MegaDrive& m,unsigned addr) {
    return addr>=0xff0000?m.ram16(addr):m.rom16(addr);
}
inline uint32_t read32(const MegaDrive& m,unsigned addr) {
    return (uint32_t(read16(m,addr))<<16)|read16(m,addr+2);
}
inline void begin(MegaDrive& m) {ensure(m);word(m,8,0);}
inline void capture(MegaDrive& m) {
    // At EB0: clipping bbox in D0..D3, sprite origin D4/D5, object A2,
    // metasprite A4. Blink/animation selection has already run unchanged.
    ensure(m);
    int x0=int16_t(m.cpu.r[0]),x1=int16_t(m.cpu.r[1]);
    int y0=int16_t(m.cpu.r[2]),y1=int16_t(m.cpu.r[3]);
    if(x0>0x1bf+40 || x1<0x80-40 || y0>int16_t(m.ram16(0xcd50)) || y1<int16_t(m.ram16(0xcd4e))) return;
    unsigned obj=m.cpu.r[10]&65535,meta=m.cpu.r[12]&0xffffff;
    if(obj>0xffe8) return;
    // Supported frames are in ROM; RAM-generated special modes use the
    // original viewport and do not enter this hook's gameplay path.
    if(!valid(m,meta,24)) return;
    unsigned parts=unsigned(m.rom16(meta))+1;
    if(parts>kCount || !valid(m,meta+24,parts*16)) return;
    unsigned count=std::min(word(m,8),kCount);
    uint16_t flags=m.ram16(obj+8)&0xf800;
    int ox=int16_t(m.cpu.r[4]),oy=int16_t(m.cpu.r[5]);
    for(unsigned i=0;i<parts && count<kCount;++i) {
        unsigned p=meta+24+i*16;
        int width=m.rom16(p),height=m.rom16(p+2);
        int x=int16_t(m.rom16(p+4)),y=int16_t(m.rom16(p+6));
        uint16_t shape=(m.rom16(p+8)>>8)&15;
        unsigned w=(((shape>>2)&3)+1)*8,h=((shape&3)+1)*8;
        unsigned source=read32(m,p+12)&0xffffff;
        if(!valid(m,source,w*h/2)) continue;
        if(flags&0x0800) x=-(x+width);
        if(flags&0x1000) y=-(y+height);
        x=int16_t(x+ox)-128;y=int16_t(y+oy)-128;
        if(x>=360 || x+int(w)<=-40 || y>=240 || y+int(h)<=0) continue;
        Sprite s{source,int16_t(x),int16_t(y),uint16_t(flags|(m.rom16(p+10)&0xf800)),shape};
        std::memcpy(m.module_data.data()+kHeader+count*sizeof(s),&s,sizeof(s));++count;
    }
    word(m,8,count);
}
inline void publish(MegaDrive& m) {
    ensure(m);
    unsigned count=std::min(word(m,8),kCount);
    std::memcpy(m.module_data.data()+kHeader+kBank,m.module_data.data()+kHeader,count*sizeof(Sprite));
    word(m,4,count);
}
inline void line(MegaDrive& m,int y,uint8_t* pixels) {
    if(m.module_data.size()!=kSize || word(m,0)!=kMagic) return;
    int e=m.vdp.ext;if(!e) return;
    std::fill(pixels,pixels+e,0);std::fill(pixels+e+320,pixels+e+320+e,0);
    unsigned count=std::min(word(m,4),kCount);
    for(unsigned i=0;i<count;++i) {
        Sprite s{};std::memcpy(&s,m.module_data.data()+kHeader+kBank+i*sizeof(s),sizeof(s));
        unsigned hc=((s.shape>>2)&3)+1,vc=(s.shape&3)+1;
        int w=int(hc*8),h=int(vc*8),row=y-s.y;
        if(row<0 || row>=h || !valid(m,s.source,unsigned(w*h/2))) continue;
        if(s.attr&0x1000) row=h-1-row;
        uint8_t attr=uint8_t(((s.attr&0x8000)?0x80:0)|((s.attr>>9)&0x30));
        for(int x=std::max(int(s.x),-e);x<std::min(int(s.x)+w,320+e);++x) {
            if(x>=0 && x<320) continue;
            if(pixels[x+e]&15) continue; // original metasprite/SAT order
            int col=x-s.x;if(s.attr&0x0800) col=w-1-col;
            unsigned offset=((col/8)*vc+unsigned(row/8))*32+(row&7)*4+(col&7)/2;
            uint8_t packed=m.rom[s.source+offset];
            uint8_t color=(col&1)?packed&15:packed>>4;
            if(color) pixels[x+e]=attr|color;
        }
    }
}
} // namespace lion_sprites
