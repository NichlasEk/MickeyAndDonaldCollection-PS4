#pragma once
#include "../core/md.h"

// Known menu backgrounds only. Hash the nametable indices, never embed artwork.
// Checking the table signatures rejects gameplay, loading and partially uploaded
// screens even when their VDP layout matches the title screen. Palette fades
// and animated pattern pixels do not change this identity.
namespace menu_wide {
inline uint32_t table_hash(const VDP& v, unsigned base) {
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<4096;++i) hash=(hash^v.vram[(base+i)&65535])*16777619u;
    return hash;
}
inline bool layout(const VDP& v) {
    return v.active_width()==320 && (v.reg[16]&0x33)==1 &&
           v.reg[17]==0 && (v.reg[18]==0 ||
           ((v.reg[18]&0x80) && (v.reg[18]&31)*8>=v.active_lines()));
}
inline bool lion(const MegaDrive& md) {
    const auto& v=md.vdp;
    if(!layout(v) || (v.reg[2]&0x38)!=0x38 || (v.reg[4]&7)!=6) return false;
    // Title and options: plane A is Pride Rock, B is the sky. All text is
    // sprites. CONTINUE has empty planes; leave its intended black backdrop.
    return table_hash(v,0xc000)==0xda418ca3u && table_hash(v,0xe000)==0x41950d58u;
}
inline bool aladdin(const MegaDrive& md) {
    const auto& v=md.vdp;
    if(!layout(v) || (v.reg[2]&0x38)!=0x30 || (v.reg[4]&7)!=7) return false;
    // Shared title/options palace and starfield. Plane A holds the logo/text.
    return table_hash(v,0xe000)==0xd903687au;
}
inline void line(const VDP& v, uint8_t* a, uint8_t* b, uint8_t* sprites, bool reflect_a) {
    const int e=v.ext,w=v.active_width();
    for(int i=0;i<e;++i) {
        const int left=e-1-i,right=e+w+i;
        const int source_left=e+i%w,source_right=e+w-1-i%w;
        a[left]=reflect_a?a[source_left]:0;
        a[right]=reflect_a?a[source_right]:0;
        b[left]=b[source_left];b[right]=b[source_right];
        // Never repeat or expose clipped menu sprites outside the original view.
        sprites[left]=sprites[right]=0;
    }
}
// Older modules use hook_flags bit 0 for HUD anchoring. Reserve bits 8..11
// for the current frame's menu policy; clear them before every frame/load.
constexpr uint32_t classic_mask=0xf00;
enum class Classic { Castle, Quack, World };
inline void classic_begin(MegaDrive& md) { md.vdp.hook_flags &= ~classic_mask; }
inline bool classic_active(const MegaDrive& md) { return (md.vdp.hook_flags&classic_mask)!=0; }
inline void classic_line(MegaDrive& md,Classic game,int y,uint8_t* a,uint8_t* b,uint8_t* sprites) {
    auto& v=md.vdp;
    if(y==0) {
        classic_begin(md);
        if(v.active_width()==320 && v.reg[17]==0 && v.reg[18]==0 &&
           (v.reg[2]&0x38)==0x30 && (v.reg[4]&7)==7) {
            const auto hb=table_hash(v,0xe000);
            if(game==Classic::Castle && (v.reg[16]&0x33)==3 && hb==0xf447d86eu)
                v.hook_flags|=0x100; // title: mountain/castle background only
            if(game==Classic::Quack && (v.reg[16]&0x33)==1 && hb==0xe1533f56u)
                v.hook_flags|=0x200; // selection/options: repeating green checks
            if(game==Classic::World && (v.reg[16]&0x33)==3 &&
               hb==0x8fc6968du && table_hash(v,0xc000)==0xc6a157c9u)
                v.hook_flags|=0x300; // title: extend the plain stage surround
        }
    }
    const auto policy=v.hook_flags&classic_mask;
    if(!policy)return;
    for(int i=0;i<v.ext;++i) {
        int l=v.ext-1-i,r=v.ext+320+i;
        a[l]=a[r]=sprites[l]=sprites[r]=0;
        if(policy==0x300) {b[l]=b[r]=0;continue;}
        int sl=policy==0x200 ? 15-i%16 : i%320;
        int sr=policy==0x200 ? 304+i%16 : 319-i%320;
        b[l]=b[v.ext+sl];b[r]=b[v.ext+sr];
    }
}

}
