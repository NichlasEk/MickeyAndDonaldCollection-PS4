// The Lion King (UEJ, CRC 5696A5BC): experimental widescreen module.
#include "game.h"
#include "common.h"
#include "wide_map.h"
#include "lion_sprites.h"
#include "../core/md.h"
#include <algorithm>
namespace {
uint16_t rw(const MegaDrive& md,unsigned a) { return md.ram16(a); }
uint32_t rl(const MegaDrive& md,unsigned a) { return (uint32_t(rw(md,a))<<16)|rw(md,a+2); }
bool playing(const MegaDrive& md) {
    unsigned cam=rw(md,0xffcd36);
    return rw(md,0xffcd40)==0 && cam>=0x9300 && cam<0xffd0 &&
        (md.vdp.reg[2]&0x38)==0x38 && (md.vdp.reg[16]&0x33)==1 &&
        rw(md,0xffcd1e)>=40 && rw(md,0xffcd1e)<=1024 && rw(md,0xffcd20)>0 && rw(md,0xffcd20)<=1024;
}
// Only replace the extension pixels. Original VRAM, collision maps, camera,
// centre image and the game's own streamer are left intact.
void line(void* host,int y,uint8_t* a,uint8_t*,uint8_t* sprites) {
    auto& view=*static_cast<WideMapView*>(host);auto& md=*view.md;auto& v=md.vdp;
    if(y==0) view.gameplay=playing(md);
    if(!playing(md)) return;
    lion_sprites::line(md,y,sprites);
    unsigned cam=rw(md,0xffcd36), stride=rw(md,0xffcd1e), rows=rw(md,0xffcd20);
    unsigned map=rl(md,0xffcd22)&0xffffff, defs=rl(md,0xffcd2a)&0xffffff;
    int cx=wide_map_x(v,y,0,rw(md,cam+8)), cy=wide_map_y(v,0,rw(md,cam+10)), wy=cy+y;
    if(y==0) {view.left=std::clamp(v.ext-cx,0,v.ext);view.right=std::clamp(cx+320+v.ext-int(stride*8),0,v.ext);}
    if(!v.ext) return;
    if(map<0xff0000 || map+stride*rows>0x1000000 || defs>=md.rom.size() || wy<0 || wy>=int(rows*16)) return;
    for(int side=0;side<2;++side) {
        int end=side?320+v.ext:0;
        for(int sx=side?320:-v.ext;sx<end;) {
            int wx=cx+sx, n=std::min(end-sx,8-(wx&7));
            if(wx<0 || wx>=int(stride*8)) { std::fill(a+sx+v.ext,a+sx+v.ext+n,0);sx+=n;continue; }
            unsigned block=rw(md,map+(wy/16)*stride+(wx/16)*2)&0xfffe;
            unsigned entry=defs+block+((wy/8)&1)*4+((wx/8)&1)*2;
            if(entry+1>=md.rom.size()) return;
            uint8_t pixels[8];v.pattern_row(md.rom16(entry),wy&7,pixels);
            std::copy(pixels+(wx&7),pixels+(wx&7)+n,a+sx+v.ext);sx+=n;
        }
    }
}
class Lion final:public GameModule {
 WideMapView view;
public:
 void pre_frame(MegaDrive&,const GameConfig&) override {view.gameplay=false;}
 const char* id() const override {return "lion";}
 const char* title() const override {return "The Lion King";}
 const char* subtitle() const override {return "Simba";}
 const char* year() const override {return "1994";}
 uint32_t rom_crc32() const override {return 0x5696a5bc;}
 uint32_t rom_size() const override {return 3145728;}
 const char* rom_hint() const override {return "The Lion King (UEJ), raw 3 MiB";}
 const char* rom_names() const override {return "LION KING";}
 uint32_t accent_color() const override {return 0xffe5b455;}

 std::vector<LevelEntry> levels() const override {
  std::vector<LevelEntry> out;
  for(const char* n:{"The Pridelands","Can't Wait to Be King","Elephant Graveyard","The Stampede","Simba's Exile","Hakuna Matata","Simba's Destiny","Be Prepared","Simba's Return","Pride Rock"}) out.push_back({"The Lion King",n});
  return out;
 }
 bool start_level(MegaDrive& md,const GameConfig& c,int index,int) override {
  if(index<0 || index>=10) return false;
  md.power_on();configure(md,c);
  md.add_pc_hook(0x1d765a,[index](MegaDrive& m){m.cpu.r[0]=index+1;});
  bool ok=false;
  for(int f=0;f<6000;++f) {
   md.set_pad(0,f%240<6?MD_START:0);md.run_frame();
   bool active=playing(md) || (rw(md,0xccce)==4 && md.vdp.active_width()==256 && md.vdp.display_enabled());
   if(active && rw(md,0xccce)==index+1) {ok=true;break;}
  }
  md.set_pad(0,0);md.clear_pc_hooks();configure(md,c);
  if(ok) for(int f=0;f<120;++f) md.run_frame();
  return ok;
 }
 void configure(MegaDrive& md,const GameConfig& c) override {
    (void)c;
    apply_patches(md,{});view.md=&md;md.vdp.hook_host=&view;md.vdp.line_hook=line;
    md.on_frame_start=nullptr;md.on_vblank=nullptr;
    lion_sprites::ensure(md);
    md.add_pc_hook(0xdce,[](MegaDrive& m){lion_sprites::begin(m);});
    md.add_pc_hook(0xeb0,[](MegaDrive& m){if(playing(m)) lion_sprites::capture(m);});
    // The original SAT DMA has just completed: present the matching snapshot.
    md.add_pc_hook(0x13be,[](MegaDrive& m){lion_sprites::publish(m);});
 }
 bool can_configure(const MegaDrive& md,const GameConfig& c) const override {(void)c;return patches_safe(md,{});}
 void post_frame(MegaDrive& md,const GameConfig& c,FrameInfo& f) override {
    (void)md;(void)c;f.gameplay=view.gameplay;
  // Keep the visible image centred at either level boundary. Session eases
  // both masks open together as the camera moves into the level.
  f.mask_left=f.mask_right=std::max(view.left,view.right);
 }
};
}
GameModule* create_lion_module() {return new Lion;}
