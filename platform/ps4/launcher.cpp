#include "launcher.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>
namespace {
constexpr int W=kLauncherWidth,H=kLauncherHeight;
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
void text(std::vector<uint32_t>& p,int x,int y,const std::string& str,uint32_t c,int scale=2) {
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
                       const std::array<PS4GameEntry,kPS4GameCount>& games,const std::string& message) {
    using Clock=std::chrono::steady_clock;
    static auto last=Clock::now();
    static float position=float(selected);
    auto now=Clock::now();
    float dt=std::min(0.05f,std::chrono::duration<float>(now-last).count()); last=now;
    float delta=float(selected)-position;
    while(delta>2.5f) delta-=5;
    while(delta< -2.5f) delta+=5;
    position+=delta*(1-std::exp(-16*dt));
    position=std::fmod(position+5,5);
    p.resize(W*H);
    const uint32_t accents[]={0xff83d6b4,0xffffc878,0xffb7a0ef,0xffefb565,0xff7ed5e5};
    uint32_t accent=accents[selected];
    for(int y=0;y<H;++y) for(int x=0;x<W;++x) {
        int glow=std::max(0,30-std::abs(x-400)/18-std::abs(y-180)/12);
        p[y*W+x]=0xff000000u|uint32_t(9+glow/3)<<16|uint32_t(15+glow/2)<<8|uint32_t(27+glow);
    }
    text(p,32,23,"MICKEY & DONALD",0xffeef1f4);
    text(p,32,46,"ALADDIN & THE LION KING",0xff8e9fb5,1);
    text(p,632,27,"GAME LIBRARY",0xff9daec4,1);
    rect(p,32,65,736,1,0xff2c3e54);
    const char* titles[]={"CASTLE OF ILLUSION","QUACKSHOT","WORLD OF ILLUSION","THE LION KING","ALADDIN - FINAL CUT"};
    const char* subtitles[]={"MICKEY MOUSE","DONALD DUCK","MICKEY AND DONALD","SIMBA / PRIDE LANDS","THE FINAL CUT"};
    std::array<int,5> order={0,1,2,3,4};
    auto distance=[&](int i) { float d=i-position; while(d>2.5f)d-=5; while(d< -2.5f)d+=5; return d; };
    std::sort(order.begin(),order.end(),[&](int a,int b){return std::abs(distance(a))>std::abs(distance(b));});
    for(int i:order) {
        float d=distance(i), focus=std::max(0.0f,1-std::abs(d));
        int cw=260+int(100*focus), ch=cw*9/16;
        int x=400+int(d*310)-cw/2, y=100+int((1-focus)*22);
        rect(p,x-5,y-5,cw+10,ch+34,0xff080d17);
        rect(p,x-2,y-2,cw+4,ch+4,i==selected?accent:0xff38495f);
        rect(p,x,y,cw,ch,0xff17263a);
        const auto& preview=games[i].preview;
        if(preview.size()==320*180) {
            for(int yy=0;yy<ch;++yy) for(int xx=0;xx<cw;++xx) {
                int px=x+xx,py=y+yy;
                if(px<0||px>=W||py<0||py>=H)continue;
                uint32_t c=preview[(yy*180/ch)*320+xx*320/cw];
                int brightness=150+int(105*focus);
                p[py*W+px]=0xff000000u|(((c>>16)&255)*brightness/255)<<16|(((c>>8)&255)*brightness/255)<<8|(c&255)*brightness/255;
            }
        } else {
            text(p,x+24,y+ch/2,games[i].ready?"PREPARING PREVIEW":"ROM NOT FOUND",0xffaebed0,1);
        }
        text(p,x+10,y+ch+10,titles[i],i==selected?0xffedf3fa:0xff9daec4,1);
    }
    text(p,(W-int(std::strlen(titles[selected]))*12)/2,343,titles[selected],0xfff2f3f6);
    text(p,(W-int(std::strlen(subtitles[selected]))*6)/2,365,subtitles[selected],0xff9baec4,1);
    for(int i=0;i<5;++i) rect(p,367+i*14,324,i==selected?10:5,3,i==selected?accent:0xff435269);
    std::string status=active==selected?"PAUSED":games[selected].ready?"READY TO PLAY":"ROM MISSING";
    if(games[selected].saved)status+=" / SAVE AVAILABLE";
    text(p,32,82,status,accent,1);
    text(p,650,82,std::to_string(selected+1)+" / 5",0xff9baec4,1);
    rect(p,32,387,736,1,0xff2c3e54);
    text(p,32,399,message.empty()?"LEFT / RIGHT  BROWSE     X  PLAY     O  BACK":message,0xffe2d3ad,1);
    text(p,32,424,"R1 SAVE   L1 LOAD   SQUARE RESTART   TRIANGLE SCAN",0xff91a5bd,1);
    text(p,650,424,"TOUCHPAD MENU",0xff91a5bd,1);
}
