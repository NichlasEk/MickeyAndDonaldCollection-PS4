// Aladdin Final Cut / ALADDIN-REMIX (CRC DD66B3FA): experimental widescreen.
#include "game.h"
#include "common.h"
#include "wide_map.h"
#include "../core/md.h"
#include <algorithm>
namespace {
bool playing(const MegaDrive& md) {
 return md.ram[0x7e2e]==0xff && ((md.vdp.reg[11]&3)==0 || (md.vdp.reg[11]&3)==2) &&
    (md.vdp.reg[2]&0x38)==0x30 && (md.vdp.reg[16]&0x33)==1 &&
    md.ram16(0x7dc8)>=320 && md.ram16(0x7dcc)>=224;
}
std::vector<RomPatch> patches(int e) {
 if(!e) return {};
 return {{0x1d6110,{0x0c43,0x01bf},{0x0c43,uint16_t(0x1bf+e)}},
         {0x1d6152,{0x0c43,0x01bf},{0x0c43,uint16_t(0x1bf+e)}}};
}
void line(void* host,int y,uint8_t* a,uint8_t* b,uint8_t*) {
 auto& view=*static_cast<WideMapView*>(host);auto& md=*view.md;auto& v=md.vdp;
 if(y==0) view.gameplay=playing(md);
 // FF7E2E is also a main-loop busy flag: it clears during active scanout.
 // Decide at line zero and keep rendering the extensions for the whole frame.
 if(!view.gameplay) return;
 // Rug Ride uses a fully populated, repeating plane, not the collision map.
 if(md.ram[0x7e36]==8) {view.left=view.right=0;return;}
 int bank=md.ram[0xf695]?1:0;
 if(bank) a=b; // Dungeon foreground lives on plane B.
 int cx=wide_map_x(v,y,bank,md.ram16(0x7e06)+16),cy=wide_map_y(v,bank,md.ram16(0x7e08)+16),wy=cy+y;
 int width=md.ram16(0x7dc8),height=md.ram16(0x7dcc);
 // The first 16 map pixels are a streaming guard column, not visible level art.
 if(y==0) {view.left=std::clamp(v.ext+16-cx,0,v.ext);view.right=std::clamp(cx+320+v.ext-width,0,v.ext);}
 if(!v.ext) return;
 unsigned defs=md.ram32(0x7dce)&0xffffff;
 if(wy<0 || wy>=height || 0x9896+(wy/16)*64+3>=0x10000 || defs>=md.rom.size()) return;
 unsigned row=md.ram32(0x9896+(wy/16)*64)&0xffffff;
 if(row<0xff0000 || row+unsigned(width/8)>0x1000000) return;
 for(int side=0;side<2;++side) {
  int end=side?320+v.ext:0;
  for(int sx=side?320:-v.ext;sx<end;) {
   int wx=cx+sx,n=std::min(end-sx,8-(wx&7));
   if(wx<16 || wx>=width) {std::fill(a+sx+v.ext,a+sx+v.ext+n,0);sx+=n;continue;}
   unsigned block=md.ram16(row+(wx/16)*2);
   unsigned entry=defs+block+((wy/8)&1)*4+((wx/8)&1)*2;
   if(entry+1>=md.rom.size()) return;
   uint8_t pixels[8];v.pattern_row(md.rom16(entry),wy&7,pixels);
   std::copy(pixels+(wx&7),pixels+(wx&7)+n,a+sx+v.ext);sx+=n;
  }
 }
}
class Aladdin final:public GameModule {
 WideMapView view;
public:
 void pre_frame(MegaDrive&,const GameConfig&) override {view.gameplay=false;}
 const char* id() const override {return "aladdin";}
 const char* title() const override {return "Aladdin - Final Cut";}
 const char* subtitle() const override {return "ALADDIN-REMIX";}
 const char* year() const override {return "2019";}
 uint32_t rom_crc32() const override {return 0xdd66b3fa;}
 uint32_t rom_size() const override {return 2384052;}
 const char* rom_hint() const override {return "Aladdin Final Cut, raw ALADDIN-REMIX";}
 const char* rom_names() const override {return "ALADDIN-REMIX";}
 uint32_t accent_color() const override {return 0xffb488eb;}

 std::vector<LevelEntry> levels() const override {
  std::vector<LevelEntry> out;
  for(const char* n:{"Agrabah","Abu Bonus","The Desert","Sultan's Dungeon","Cave of Wonders","Abu in the Cave","The Escape","Rug Ride","Inside the Lamp","Sultan's Palace","Jafar's Palace"}) out.push_back({"Aladdin Final Cut",n});
  return out;
 }
 bool start_level(MegaDrive& md,const GameConfig& c,int index,int) override {
  if(index<0 || index>=11) return false;
  md.power_on();configure(md,c);
  md.add_pc_hook(0x1d4a96,[index](MegaDrive& m){m.ram[0x7e36]=uint8_t(index+1);});
  bool ok=false;
  for(int f=0;f<6000;++f) {
   md.set_pad(0,f%240<6?MD_START:0);md.run_frame();
   if(playing(md) && md.ram[0x7e36]==index+1) {ok=true;break;}
  }
  md.set_pad(0,0);md.clear_pc_hooks();
  if(ok) for(int f=0;f<120;++f) md.run_frame();
  return ok;
 }
 void configure(MegaDrive& md,const GameConfig& c) override {
  apply_patches(md,patches(c.ext));view.md=&md;md.vdp.hook_host=&view;md.vdp.line_hook=line;
 }
 bool can_configure(const MegaDrive& md,const GameConfig& c) const override {return patches_safe(md,patches(c.ext));}
 void post_frame(MegaDrive& md,const GameConfig& c,FrameInfo& f) override {
  (void)md;(void)c;f.gameplay=view.gameplay;
  // Keep the visible image centred at either level boundary. Session eases
  // both masks open together as the camera moves into the level.
  f.mask_left=f.mask_right=std::max(view.left,view.right);
 }
};
}
GameModule* create_aladdin_module() {return new Aladdin;}
