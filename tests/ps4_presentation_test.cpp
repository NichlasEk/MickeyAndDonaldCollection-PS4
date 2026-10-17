#include "../platform/ps4/presentation.h"
#include <gnm_helpers.h>
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>
#include <deque>
#include <cstdio>
#include <stdexcept>
extern "C" GnmError __wrap_sceGnmVideoOutSubmitFlipAndWait(GnmVideoOut*,uint32_t,int64_t,int32_t);
namespace {
std::deque<OrbisVideoOutFlipStatus> pending;
OrbisVideoOutFlipStatus displayed{0,-1};
uint64_t now=0;
int waits=0;
bool fail_submit=false;
void require(bool value,const char* text) { if(!value) throw std::runtime_error(text); }
}
uint64_t sceKernelGetProcessTime() { return now; }
int sceVideoOutSubmitFlip(int,int index,uint32_t,int64_t arg) {
    if(fail_submit) return -77;
    pending.push_back({arg,index}); return 0;
}
int sceVideoOutGetFlipStatus(int,OrbisVideoOutFlipStatus* status) { *status=displayed; return 0; }
int sceKernelWaitEqueue(OrbisKernelEqueue,OrbisKernelEvent*,int,int* count,OrbisKernelUseconds* timeout) {
    ++waits; now+=*timeout;
    if(pending.empty()) { *count=0; return int(ORBIS_KERNEL_ERROR_ETIMEDOUT); }
    displayed=pending.front(); pending.pop_front(); *count=1; return 0;
}
int main() {
    try {
        GnmVideoOut output{1,1,3,0,0};
        auto submit=[&](unsigned i) { require(__wrap_sceGnmVideoOutSubmitFlipAndWait(&output,i,0,1)==GNM_ERROR_OK,"submit failed"); };
        ps4_wait_image_reusable(0); require(waits==0,"unused buffer blocked");
        submit(0); submit(1); submit(2);
        require(waits==0,"queueing flips blocked");
        ps4_wait_image_reusable(0);
        require(waits==2 && displayed.currentBuffer==1,"image reused before later flip retired it");
        ps4_wait_image_reusable(1);
        require(waits==3 && displayed.currentBuffer==2,"scanned-out image reused");
        // Latest image remains owned by display until a subsequent flip, with a bounded timeout.
        bool timed_out=false;
        try { ps4_wait_image_reusable(2); } catch(const std::runtime_error&) { timed_out=true; }
        require(timed_out && now<2100000,"stalled display was not bounded");
        submit(0); ps4_wait_image_reusable(2);
        require(displayed.currentBuffer==0,"retirement did not recover after later flip");
        fail_submit=true;
        require(__wrap_sceGnmVideoOutSubmitFlipAndWait(&output,1,0,1)!=GNM_ERROR_OK,"native error lost");
        bool propagated=false;
        try { ps4_check_present(); } catch(const std::runtime_error&) { propagated=true; }
        require(propagated,"ICD-ignored flip error was not propagated");
        std::puts("PASS: nonblocking enqueue, scanout retirement, bounded wait and native error propagation");
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL %s\n",e.what()); return 1; }
}
