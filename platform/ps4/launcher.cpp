#include "launcher.h"
#include <algorithm>
#include <cstring>
namespace {
constexpr int W=400,H=224;
const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/:.?&";
const uint8_t glyphs[][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{0,0,0,31,0,0,0},{1,2,2,4,8,8,16},
 {0,4,4,0,4,4,0},{0,0,0,0,0,12,12},{14,17,1,2,4,0,4},
 {12,18,20,8,21,18,13}
};
static_assert(sizeof(glyphs)/sizeof(glyphs[0])==sizeof(chars)-1,"font map");
void rect(std::vector<uint32_t>& p,int x,int y,int w,int h,uint32_t c) {
    for(int yy=std::max(0,y);yy<std::min(H,y+h);++yy)
        for(int xx=std::max(0,x);xx<std::min(W,x+w);++xx) p[yy*W+xx]=c;
}
void text(std::vector<uint32_t>& p,int x,int y,const std::string& str,uint32_t c,int scale=1) {
    for(unsigned char ch:str) {
        if(ch>='a' && ch<='z') ch-=32;
        const char* at=std::strchr(chars,ch);
        if(at && ch) for(int row=0;row<7;++row) for(int col=0;col<5;++col)
            if(glyphs[at-chars][row]&(16>>col)) rect(p,x+col*scale,y+row*scale,scale,scale,c);
        x+=6*scale; if(x>=W-12) break;
    }
}
}
void ps4_draw_launcher(std::vector<uint32_t>& p,int selected,int active,
                       const std::array<PS4GameEntry,3>& games,const std::string& message) {
    p.resize(W*H);
    for(int y=0;y<H;++y) for(int x=0;x<W;++x)
        p[y*W+x]=0xff000000u|uint32_t(10+y/32)<<16|uint32_t(16+y/20)<<8|uint32_t(32+y/12);
    text(p,18,14,"MICKEY & DONALD",0xffffd580,2);
    text(p,19,33,"COLLECTION / GAME LIBRARY",0xffa6bdd4);
    text(p,356,18,"PS4",0xffa6bdd4);
    const char* titles[]={"CASTLE OF ILLUSION","QUACKSHOT","WORLD OF ILLUSION"};
    const char* subtitles[]={"MICKEY MOUSE","DONALD DUCK","MICKEY MOUSE AND DONALD DUCK"};
    for(int i=0;i<3;++i) {
        int y=51+i*43; bool chosen=selected==i;
        uint32_t accent=chosen?0xffffd580:0xff36506c;
        rect(p,16,y,368,39,accent); rect(p,17,y+1,366,37,chosen?0xff263c57:0xff15273d);
        rect(p,24,y+9,21,21,accent);
        text(p,32,y+16,std::to_string(i+1),chosen?0xff18283c:0xffc7d9e9);
        text(p,54,y+8,titles[i],chosen?0xffffe3ab:0xffe4ecf4);
        text(p,54,y+24,subtitles[i],0xff91a9c3);
        if(games[i].saved) text(p,346,y+24,"SAVE",0xff8edcaa);
        std::string status=active==i?"PAUSED":games[i].source;
        text(p,376-int(status.size())*6,y+8,status,games[i].ready?0xff8edcaa:0xffee967d);
    }
    text(p,18,185,message.empty()?"SELECT A GAME - HDD FIRST / USB FALLBACK":message,0xffb9cddd);
    text(p,18,201,"X PLAY  O BACK  SQUARE RESTART  TRIANGLE SCAN",0xffffd580);
    text(p,18,214,"R1 SAVE  L1 LOAD / IN GAME: TOUCHPAD MENU",0xff91a9c3);
}
