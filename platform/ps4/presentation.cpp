// App-local adapter for the current OpenGNM ICD. No shared dependency is modified.
// Queue flips immediately; retire an image only AFTER a later flip completed.
// Four images allow queued flips while the next frame is rendered.
#include "presentation.h"
#include <gnm_helpers.h>
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>
#include <array>
#include <stdexcept>
#include <string>
namespace {
GnmVideoOut* owner=nullptr;
std::array<int64_t,GNM_VIDEO_OUT_MAX_BUFFERS> submitted{};
int64_t serial=0;
int error=0;
}
extern "C" GnmError __wrap_sceGnmVideoOutSubmitFlipAndWait(GnmVideoOut* output,uint32_t index,int64_t,int32_t mode) {
    if(!output || output->handle<0 || index>=output->numbuffers || index>=submitted.size() || (owner && owner!=output)) {
        error=-1; return GNM_ERROR_INVALID_ARGS;
    }
    owner=output;
    int result=sceVideoOutSubmitFlip(output->handle,int(index),mode,serial+1);
    if(result!=0) { error=result; return static_cast<GnmError>(result); }
    submitted[index]=++serial;
    ++output->frame;
    output->currentbuffer=(index+1)%output->numbuffers;
    return GNM_ERROR_OK;
}
void ps4_check_present() {
    if(error) throw std::runtime_error("VideoOut submission failed: "+std::to_string(error));
}
void ps4_wait_image_reusable(uint32_t index) {
    ps4_check_present();
    if(index>=submitted.size()) throw std::runtime_error("Invalid presentation image");
    if(!owner || !submitted[index]) return;
    uint64_t deadline=sceKernelGetProcessTime()+2000000;
    for(;;) {
        OrbisVideoOutFlipStatus status{};
        int result=sceVideoOutGetFlipStatus(owner->handle,&status);
        if(result) throw std::runtime_error("VideoOut status failed: "+std::to_string(result));
        // Equal means still scanned out; less means queued but not displayed yet.
        if(status.flipArg>submitted[index] && status.currentBuffer!=int(index)) return;
        if(sceKernelGetProcessTime()>=deadline) throw std::runtime_error("VideoOut image retirement timed out");
        OrbisKernelEvent event{}; int count=0; OrbisKernelUseconds timeout=20000;
        result=sceKernelWaitEqueue(reinterpret_cast<OrbisKernelEqueue>(owner->flipqueue),&event,1,&count,&timeout);
        // A timed wait is only a wakeup hint. Flip status is the ownership authority.
        // The absolute deadline also bounds a lost event or a stalled display.
        if(result && uint32_t(result)!=ORBIS_KERNEL_ERROR_ETIMEDOUT) {
            throw std::runtime_error("VideoOut event wait failed: "+std::to_string(result));
        }
    }
}
