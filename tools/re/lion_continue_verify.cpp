// Private-ROM regression. The fixture is a Session state saved at CONTINUE?,
// without the outer PS4 state-store envelope. Never commit the ROM or fixture.
#include "../../platform/ps4/rom.h"
#include <fstream>
#include <iostream>
#include <iterator>
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
 try {
  require(argc==3,"Usage: lion_continue_verify ROM SESSION_STATE");
  auto rom=load_rom_file(argv[1]);std::ifstream file(argv[2],std::ios::binary);
  std::vector<uint8_t> state((std::istreambuf_iterator<char>(file)),{});
  auto wg=identify_rom(rom),ng=identify_rom(rom);Session wide,narrow;
  require(wide.open(wg.get(),rom)&&narrow.open(ng.get(),rom),"open failed");
  Settings ws,ns;ns.widescreen=WideMode::Off;wide.apply_settings(ws);narrow.apply_settings(ns);
  require(wide.load_state(state)&&narrow.load_state(state),"fixture load failed");
  bool resumed=false;unsigned menu_frames=0,game_frames=0;
  for(int f=0;f<1200;++f) {
   // Allow the menu to animate, then choose its current YES selection.
   unsigned pad=(f>=90 && f<96)?MD_START:0;
   wide.run_frame(pad,0);narrow.run_frame(pad,0);
   int w,h,nw,nh;const auto* a=wide.frame(w,h);const auto* b=narrow.frame(nw,nh);
   require(w==nw+80&&h==nh,"unexpected dimensions");
   for(int y=0;y<h;++y)for(int x=0;x<nw;++x)
    require(a[y*w+x+40]==b[y*nw+x],"original centre changed");
   if(f<90) {
    require(!wide.info().gameplay,"continue misclassified as gameplay");
    for(int y=0;y<h;++y)for(int x=0;x<40;++x)
     require(a[y*w+x]==0xff000000u && a[y*w+w-1-x]==0xff000000u,"continue side corruption");
    ++menu_frames;
   }
   if(f>=96 && wide.info().gameplay){resumed=true;++game_frames;}
  }
  require(resumed,"YES did not restore gameplay widescreen");
  auto saved=wide.save_state();wide.run_frame(0,0);auto expected=wide.save_state();
  require(wide.load_state(saved),"roundtrip load failed");wide.run_frame(0,0);
  require(wide.save_state()==expected,"state replay changed");
  std::cout<<"PASS: "<<menu_frames<<" clean continue frames, "<<game_frames
           <<" resumed gameplay frames, unchanged centre for 1200 frames, state replay\n";
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
