// Private-ROM regression: observe level entry without start_level's warmup.
#include "../../platform/ps4/rom.h"
#include <cstring>
#include <iostream>
static void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {try {
 check(argc==2,"Usage: fade_wide_verify ROM");
 auto rom=load_rom_file(argv[1]);auto game=identify_rom(rom);
 check(bool(game),"unknown ROM");
 bool lion=std::string(game->id())=="lion";
 check(lion || std::string(game->id())=="aladdin","unsupported game");
 for(int level=0;level<int(game->levels().size());++level) {
  auto wg=identify_rom(rom),ng=identify_rom(rom);Session w,n;
  check(w.open(wg.get(),rom)&&n.open(ng.get(),rom),"open failed");
  w.apply_settings(Settings{});n.apply_settings(Settings{});
  // Same ROM patches and input; the reference only omits extension rendering.
  n.md().vdp.ext=0;
  for(auto* s:{&w,&n}) {
   if(lion)s->md().add_pc_hook(0x1d765a,[level](MegaDrive& m){m.cpu.r[0]=level+1;});
   else s->md().add_pc_hook(0x1d4a96,[level](MegaDrive& m){m.ram[0x7e36]=level+1;});
  }
  int entry=-1,visible=-1,visible_frames=0;
  std::vector<uint8_t> fade_state;
  for(int f=0;f<6000;++f) {
   unsigned pad=entry<0 && f%240<6?MD_START:0;
   w.run_frame(pad,0);n.run_frame(pad,0);
   check(!std::memcmp(w.md().ram,n.md().ram,65536),"RAM changed");
   check(w.md().cpu.pc==n.md().cpu.pc,"CPU timing changed");
   if(entry<0 && w.info().gameplay)entry=f;
   if(entry<0)continue;
   auto& a=w.md().vdp;auto& b=n.md().vdp;
   int offset=(a.fb_width-b.fb_width)/2;
   check(a.fb_lines==b.fb_lines&&offset>=0,"dimensions differ");
   for(int y=0;y<a.fb_lines;++y)for(int x=0;x<b.fb_width;++x)
    check(a.fb[y][x+offset]==b.fb[y][x],"original centre changed");
   int width,height;auto pixels=w.frame(width,height);int centre=0,sides=0;
   for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(pixels[y*width+x]!=0xff000000u) {
    if(x<offset || x>=width-offset)++sides;else ++centre;
   }
   if(centre) {
    if(visible<0){visible=f;fade_state=w.save_state();}
    check(w.info().boundary_widescreen && sides>0,"visible level starts with masked sides");
    ++visible_frames;
   }
   if(f-entry>=180)break;
  }
  check(visible_frames>0,"no visible level tested");
  // Load directly into the first visible frame, including the palette fade.
  check(w.load_state(fade_state),"fade state load failed");
  for(int i=0;i<30;++i)w.run_frame(0,0);
  auto expected=w.save_state();int width,height;auto p=w.frame(width,height);
  std::vector<uint32_t> image(p,p+width*height);
  check(w.load_state(fade_state),"fade state reload failed");
  for(int i=0;i<30;++i)w.run_frame(0,0);
  check(w.save_state()==expected,"state replay changed");p=w.frame(width,height);
  check(image==std::vector<uint32_t>(p,p+width*height),"image replay changed");
  std::cout<<"PASS level="<<level<<" entry="<<entry<<" first_visible="<<visible
           <<" visible_frames="<<visible_frames<<" immediate sides; centre/RAM/PC exact; replay\n"<<std::flush;
 }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
