// Local-ROM menu regression. Fixtures and captures stay outside public Git.
#include "../../platform/ps4/rom.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstring>
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static void capture(Session& s,const std::string& path) {
 int w,h;auto p=s.frame(w,h);std::ofstream out(path,std::ios::binary);
 out<<"P6\n"<<w<<' '<<h<<"\n255\n";
 for(int i=0;i<w*h;++i) {char c[]={char(p[i]>>16),char(p[i]>>8),char(p[i])};out.write(c,3);}
 require(bool(out),"capture failed");
}
int main(int argc,char** argv) {
 try {
  require(argc==5,"Usage: menu_wide_verify ROM CORE_STATE EXPECT_MENU OUTPUT_PREFIX");
  auto rom=load_rom_file(argv[1]);auto wg=identify_rom(rom),ng=identify_rom(rom);
  require(bool(wg)&&bool(ng),"unknown ROM");Session wide,narrow;
  require(wide.open(wg.get(),rom)&&narrow.open(ng.get(),rom),"open failed");
  Settings ws,ns;ns.widescreen=WideMode::Off;wide.apply_settings(ws);narrow.apply_settings(ns);
  std::ifstream file(argv[2],std::ios::binary);require(bool(file),"fixture open failed");
  std::vector<uint8_t> state((std::istreambuf_iterator<char>(file)),{});
  std::string tag=std::string(wg->id())+"|0|";
  state.insert(state.end(),tag.begin(),tag.end());state.push_back(uint8_t(tag.size()));
  require(wide.load_state(state)&&narrow.load_state(state),"fixture load failed");
  // Diagnostics may have saved a layer-isolation setting. Always compare the
  // complete image with identical sprite-limit settings in both sessions.
  wide.md().vdp.debug_layers=narrow.md().vdp.debug_layers=7;
  wide.md().vdp.sprite_limit=narrow.md().vdp.sprite_limit=false;
  const bool original_timing=std::string(wg->id())=="lion" || std::string(wg->id())=="aladdin";
  unsigned menu_frames=0;uint64_t side_pixels=0;
  for(int f=0;f<180;++f) {
   wide.run_frame(0,0);narrow.run_frame(0,0);
   int w,h,nw,nh;const auto* a=wide.frame(w,h);const auto* b=narrow.frame(nw,nh);
   require(w==nw+80&&h==nh,"unexpected dimensions");
   for(int y=0;y<h;++y)for(int x=0;x<nw;++x)
    require(a[y*w+x+40]==b[y*nw+x],"original centre changed");
   if(original_timing) require(!std::memcmp(wide.md().ram,narrow.md().ram,sizeof(wide.md().ram)),"emulated RAM changed");
   if(original_timing) require(wide.md().cpu.pc==narrow.md().cpu.pc,"CPU timing changed");
   require(!wide.info().gameplay,"menu misclassified as gameplay");
   if(wide.info().menu_widescreen) {
    ++menu_frames;
    for(int y=0;y<h;++y)for(int x=0;x<40;++x)
     side_pixels+=(a[y*w+x]!=0xff000000u)+(a[y*w+w-1-x]!=0xff000000u);
   } else {
    for(int y=0;y<h;++y)for(int x=0;x<40;++x)
     require(a[y*w+x]==0xff000000u&&a[y*w+w-1-x]==0xff000000u,"unrecognised menu exposed stale sides");
   }
   if(f==60){capture(wide,std::string(argv[4])+"-wide.ppm");capture(narrow,std::string(argv[4])+"-narrow.ppm");}
  }
  if(std::atoi(argv[3]))require(menu_frames==180&&side_pixels>0,"menu extension missing");
  else require(menu_frames==0,"unsupported scene unexpectedly widened");
  auto saved=wide.save_state();wide.run_frame(0,0);auto expected=wide.save_state();
  int w,h;auto pixels=wide.frame(w,h);std::vector<uint32_t> expected_pixels(pixels,pixels+w*h);
  require(wide.load_state(saved),"roundtrip load failed");wide.run_frame(0,0);
  require(wide.save_state()==expected,"state replay changed");pixels=wide.frame(w,h);
  require(std::vector<uint32_t>(pixels,pixels+w*h)==expected_pixels,"image replay changed");
  // Returning to 4:3 and enabling widescreen again must not retain an old mask.
  wide.apply_settings(ns);wide.run_frame(0,0);wide.frame(w,h);require(w==320,"4:3 switch failed");
  wide.apply_settings(ws);wide.run_frame(0,0);wide.frame(w,h);require(w==400,"wide switch failed");
  std::cout<<"PASS: "<<wg->id()<<", 180 centre comparisons, "<<menu_frames
           <<" wide menu frames, "<<side_pixels<<" nonblack side pixels, state/image replay, mode toggle\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
