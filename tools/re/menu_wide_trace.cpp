// Compile against both the baseline and candidate source trees. Equal output
// proves identical emulated state and original centre at the same wide setting;
// the older games' existing wide ROM patches deliberately differ from 4:3.
#include "platform/ps4/rom.h"
#include <fstream>
#include <iostream>
#include <iterator>
static uint64_t hash(const void* p,size_t n) {
 auto b=static_cast<const uint8_t*>(p);uint64_t h=14695981039346656037ull;
 for(size_t i=0;i<n;++i)h=(h^b[i])*1099511628211ull;
 return h;
}
int main(int argc,char** argv) {
 if(argc!=3 && argc!=4)return 2;
 auto rom=load_rom_file(argv[1]);auto game=identify_rom(rom);Session s;
 if(!game||!s.open(game.get(),rom))return 3;
 Settings settings;s.apply_settings(settings);
 std::ifstream f(argv[2],std::ios::binary);if(!f)return 4;
 std::vector<uint8_t> state((std::istreambuf_iterator<char>(f)),{});
 std::string tag=std::string(game->id())+"|0|";
 state.insert(state.end(),tag.begin(),tag.end());state.push_back(uint8_t(tag.size()));
 if(!s.load_state(state))return 5;
 s.md().vdp.debug_layers=7;s.md().vdp.sprite_limit=false;
 const bool transition=argc==4;
 for(int i=0;i<(transition?1200:180);++i) {
  s.run_frame(transition && i>=60 && i<66 ? MD_START : 0,0);int w,h;auto pixels=s.frame(w,h);std::vector<uint32_t> centre;
  for(int y=0;y<h;++y)centre.insert(centre.end(),pixels+y*w+40,pixels+y*w+w-40);
  auto saved=s.save_state();
  std::cout<<std::hex<<hash(saved.data(),saved.size())<<' '<<hash(centre.data(),centre.size()*4)<<'\n';
 }
}
