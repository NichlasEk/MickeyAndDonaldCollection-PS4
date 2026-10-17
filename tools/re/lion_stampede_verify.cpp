// Private-ROM integration check: extension must not move/change the original
// pixels or the emulated machine. Also covers the separately requested Pride Rock.
#include "../../platform/ps4/rom.h"
#include <iostream>
#include <cstring>
#include <algorithm>
static void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
int main(int argc,char**argv){try{
 require(argc==2,"Usage: lion_stampede_verify LION_ROM");auto rom=load_rom_file(argv[1]);
 for(int level:{3,9}) {
  auto wg=identify_rom(rom),ng=identify_rom(rom);Session w,n;w.open(wg.get(),rom);n.open(ng.get(),rom);
  Settings ws,ns;ns.widescreen=WideMode::Off;w.apply_settings(ws);n.apply_settings(ns);
  require(w.start_level(level)&&n.start_level(level),"level start failed");
  int gameplay=0;size_t scenery=0;
  for(int f=0;f<1200;++f){
   unsigned pad=((f/60)%2?MD_LEFT:MD_RIGHT)|(f%80<12?MD_C:0);
   w.run_frame(pad,0);n.run_frame(pad,0);
   require(!std::memcmp(w.md().ram,n.md().ram,65536),"emulated RAM differs");
   require(w.md().cpu.pc==n.md().cpu.pc,"CPU PC differs");
   int ww,wh,nw,nh;auto a=w.frame(ww,wh);auto b=n.frame(nw,nh);
   require(wh==nh && ww>=nw && (ww-nw)%2==0,"frame dimensions differ");int offset=(ww-nw)/2;
   for(int y=0;y<wh;++y)for(int x=0;x<nw;++x)require(a[y*ww+offset+x]==b[y*nw+x],"original centre differs");
   if(w.info().gameplay){++gameplay;if(ww!=400)std::cerr<<"level="<<level<<" frame="<<f<<" width="<<ww<<" active="<<w.md().vdp.active_width()<<" ext="<<w.md().vdp.ext<<" flag="<<w.md().ram16(0xcd40)<<"\n";require(ww==400,"widescreen is not 400px");
    for(int y=24;y<184;++y)for(int x=0;x<offset;++x)scenery+=a[y*ww+x]!=0xff000000u;
   }
  }
  require(gameplay>100 && scenery>0,"no extended scenery exercised");
  auto save=w.save_state();for(int i=0;i<30;++i)w.run_frame(0,0);auto expected=w.save_state();
  int fw,fh;auto fp=w.frame(fw,fh);std::vector<uint32_t> pixels(fp,fp+fw*fh);
  require(w.load_state(save),"state load failed");for(int i=0;i<30;++i)w.run_frame(0,0);
  require(w.save_state()==expected,"state replay differs");fp=w.frame(fw,fh);
  require(pixels.size()==size_t(fw*fh)&&std::equal(pixels.begin(),pixels.end(),fp),"image replay differs");
  std::cout<<"PASS level="<<level<<" gameplay_frames="<<gameplay<<" side_pixels="<<scenery<<" centre/RAM/PC exact for 1200 frames; state/image replay\n";
 }
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
