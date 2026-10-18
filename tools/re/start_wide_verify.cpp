// Private-ROM regression: initial scenery, camera movement and save replay.
#include "../../platform/ps4/rom.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
static void check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
static void capture(Session& s,const std::string& file) {
 int w,h;auto p=s.frame(w,h);std::ofstream f(file,std::ios::binary);f<<"P6\n"<<w<<' '<<h<<"\n255\n";
 for(int i=0;i<w*h;++i){char c[]={char(p[i]>>16),char(p[i]>>8),char(p[i])};f.write(c,3);}
 check(bool(f),"capture failed");
}
int main(int argc,char** argv){try{
 check(argc==3,"Usage: start_wide_verify ROM OUTPUT_PREFIX");auto rom=load_rom_file(argv[1]);auto game=identify_rom(rom);check(bool(game),"unknown ROM");
 for(int level=0;level<int(game->levels().size());++level){
  auto wg=identify_rom(rom),ng=identify_rom(rom);Session w,n;check(w.open(wg.get(),rom)&&n.open(ng.get(),rom),"open failed");
  Settings settings;w.apply_settings(settings);n.apply_settings(settings);check(w.start_level(level),"level start failed");
  check(n.load_state(w.save_state()),"reference load failed");n.md().vdp.ext=0;
  int filled=0,playing=0;uint64_t side=0;
  for(int f=0;f<600;++f){
   unsigned pad=f<120?0:((f<480?MD_RIGHT:MD_LEFT)|(f%80<12?MD_C:0));
   w.run_frame(pad,0);n.run_frame(pad,0);
   check(!std::memcmp(w.md().ram,n.md().ram,65536),"emulated RAM changed");check(w.md().cpu.pc==n.md().cpu.pc,"CPU timing changed");
   auto& a=w.md().vdp;auto& b=n.md().vdp;int offset=(a.fb_width-b.fb_width)/2;
   check(a.fb_lines==b.fb_lines&&offset>=0,"dimensions differ");
   for(int y=0;y<a.fb_lines;++y)for(int x=0;x<b.fb_width;++x)check(a.fb[y][x+offset]==b.fb[y][x],"original centre changed");
   if(w.info().gameplay){++playing;if(w.info().boundary_widescreen){++filled;check(w.info().mask_left==0&&w.info().mask_right==0,"filled scenery masked");}}
   if(f==0){
    capture(w,std::string(argv[2])+"-"+std::to_string(level)+"-start.ppm");
    int ww,hh;auto p=w.frame(ww,hh);for(int y=0;y<hh;++y)for(int x=0;x<offset;++x)side+=(p[y*ww+x]!=0xff000000u)+(p[y*ww+ww-1-x]!=0xff000000u);
   }
   if(f==300)capture(w,std::string(argv[2])+"-"+std::to_string(level)+"-scroll.ppm");
  }
  auto save=w.save_state();for(int i=0;i<30;++i)w.run_frame(0,0);auto expected=w.save_state();int ww,hh;auto p=w.frame(ww,hh);std::vector<uint32_t> image(p,p+ww*hh);
  check(w.load_state(save),"replay load failed");for(int i=0;i<30;++i)w.run_frame(0,0);check(w.save_state()==expected,"state replay changed");p=w.frame(ww,hh);check(image==std::vector<uint32_t>(p,p+ww*hh),"image replay changed");
  check(playing>0,"no gameplay tested");
  check(side>0,"initial sides are still fully masked");
  std::cout<<"PASS level="<<level<<" gameplay="<<playing<<" filled="<<filled<<" initial_side_pixels="<<side<<" centre/RAM/PC exact; state/image replay\n"<<std::flush;
 }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
