// ROM-private causal regression: the former ROM clipping patch must reproduce
// divergence. The presentation-only sprite path must preserve the original
// game updates, sampled input and centre pixels, including long runs.
#include "../../platform/ps4/rom.h"
#include <iostream>
#include <array>

struct Runner {
    std::unique_ptr<GameModule> game{create_lion_module()};
    std::unique_ptr<MegaDrive> md{new MegaDrive};
    GameConfig cfg;
    std::vector<std::array<uint16_t,2>> updates;
    std::vector<std::array<uint32_t,5>> inputs;
    Runner(const std::vector<uint8_t>& rom, const std::vector<uint8_t>& state, int ext, bool presentation_only) {
        if(!md->load_rom(rom)) throw std::runtime_error("ROM load failed");
        md->power_on();
        if(!md->load_state(state)) throw std::runtime_error("Fixture state load failed");
        cfg.ext=ext;
        md->vdp.ext=ext; md->vdp.sprite_limit=false;
        md->vdp.ignore_left_blank=ext>0;
        game->configure(*md,cfg);
        md->add_pc_hook(0xdca,[this](MegaDrive&){updates.push_back(camera());});
        md->add_pc_hook(0xc44,[this](MegaDrive& m){if((m.cpu.r[9]&65535)==0x819c) inputs.push_back({m.cpu.r[0]&255,uint32_t(m.frame_count),uint32_t(m.vdp.line),m.ram16(0xcd40),m.ram8(0x81b6)});});
        if(!presentation_only && ext) {
            // Negative control: reproduce the former ROM clipping expansion.
            md->patch_rom16(0xeb2,uint16_t(0x1bf+ext));
            md->patch_rom16(0xec4,uint16_t(0x80-ext));
        }
    }
    void frame(int n) {
        unsigned pad=(n%240<120?MD_RIGHT:MD_LEFT)|((n/30)%2?MD_C:0);
        md->set_pad(0,pad);md->set_pad(1,0);
        game->pre_frame(*md,cfg);md->run_frame();
        FrameInfo info;game->post_frame(*md,cfg,info);
    }
    uint64_t pixels() {
        uint64_t hash=14695981039346656037ull;
        auto& v=md->vdp;
        for(int y=0;y<v.fb_lines;++y) for(int x=0;x<v.fb_width;++x) {hash^=v.fb[y][x];hash*=1099511628211ull;}
        return hash;
    }
    std::array<uint16_t,2> camera() {
        unsigned p=md->ram16(0xcd36);
        return {md->ram16(p+8),md->ram16(p+10)};
    }
};
int main(int argc,char** argv) {
    if(argc!=2 && argc!=3) {std::cerr<<"Usage: lion_input_verify LION_ROM [FRAMES]\n";return 2;}
    try {
        int frames=argc==3?std::stoi(argv[2]):1200;
        if(frames<240 || frames>10000) throw std::runtime_error("Use 240..10000 frames");
        auto rom=load_rom_file(argv[1]);auto identity=identify_rom(rom);
        if(std::string(identity->id())!="lion") throw std::runtime_error("Exact supported Lion King ROM required");
        auto boot=std::make_unique<MegaDrive>();boot->load_rom(rom);
        GameConfig cfg;cfg.ext=40;boot->power_on();boot->vdp.ext=40;boot->vdp.sprite_limit=false;
        boot->vdp.ignore_left_blank=true;identity->configure(*boot,cfg);
        if(!identity->start_level(*boot,cfg,2,0)) throw std::runtime_error("Level boot failed");
        auto state=boot->save_state();
        for(bool presentation_only:{false,true}) {
            Runner wide(rom,state,40,presentation_only),original(rom,state,0,presentation_only);
            int camera_diffs=0,center_frames=0,first=-1;
            uint64_t changed=0;
            for(int frame=0;frame<frames;++frame) {
                wide.frame(frame);original.frame(frame);
                if(wide.camera()!=original.camera()) {++camera_diffs;if(first<0) first=frame;}
                auto& a=wide.md->vdp;auto& b=original.md->vdp;
                uint64_t pixels=0;
                for(int y=0;y<b.fb_lines;++y) for(int x=0;x<b.fb_width;++x)
                    pixels+=a.fb[y][x+40]!=b.fb[y][x];
                changed+=pixels;center_frames+=pixels>0;
            }
            std::cout<<"presentation_only="<<presentation_only<<" frames="<<frames<<" camera_diffs="<<camera_diffs
                     <<" first_camera_diff="<<first<<" center_frames="<<center_frames
                     <<" changed_pixels="<<changed<<std::endl;
            size_t update_diff=0,input_diff=0;int first_update=-1,first_input=-1;
            for(size_t i=0;i<std::min(wide.updates.size(),original.updates.size());++i) if(wide.updates[i]!=original.updates[i]) {++update_diff;if(first_update<0) first_update=int(i);}
            for(size_t i=0;i<std::min(wide.inputs.size(),original.inputs.size());++i) if(wide.inputs[i][0]!=original.inputs[i][0]) {++input_diff;if(first_input<0) first_input=int(i);}
            std::cout<<"updates="<<wide.updates.size()<<","<<original.updates.size()<<" differing_updates="<<update_diff
                     <<" inputs="<<wide.inputs.size()<<","<<original.inputs.size()<<" differing_inputs="<<input_diff<<" first_update="<<first_update<<" first_input="<<first_input<<std::endl;
            if(first_input>=0) {
                for(auto* r:{&wide,&original}) {auto p=r->inputs[first_input];std::cout<<"first_input_sample buttons="<<p[0]<<" frame="<<p[1]<<" line="<<p[2]<<" mode="<<p[3]<<" blocked="<<p[4]<<std::endl;}
            }
            if(!presentation_only && !input_diff) throw std::runtime_error("Negative control did not reproduce input divergence");
            if(presentation_only && (update_diff || input_diff || center_frames || wide.updates.size()!=original.updates.size() || wide.inputs.size()!=original.inputs.size())) throw std::runtime_error("Presentation-only trajectory diverged");
            if(presentation_only) {
                auto saved=wide.md->save_state();
                std::vector<uint64_t> images;
                for(int f=0;f<90;++f) {wide.frame(f);images.push_back(wide.pixels());}
                auto expected=wide.md->save_state();
                Runner restored(rom,saved,40,true);
                for(int f=0;f<90;++f) {
                    restored.frame(f);
                    if(restored.pixels()!=images[f]) throw std::runtime_error("Fresh-session image replay diverged");
                }
                if(restored.md->save_state()!=expected) throw std::runtime_error("Fresh-session input replay diverged");
                std::cout<<"PASS fresh-session state and full-width image replay with changing input, 90 frames\n";
            }
        }
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
