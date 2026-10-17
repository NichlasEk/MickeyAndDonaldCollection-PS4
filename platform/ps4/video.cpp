// Minimal native Vulkan presenter. The PS4 ICD owns VideoOut and accepts a null surface.
#include "video.h"
#include "presentation.h"
#include "shaders.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <orbis/libkernel.h>
namespace {
void check(VkResult r, const char* operation) {
    if (r != VK_SUCCESS) throw std::runtime_error(std::string(operation) + ": " + std::to_string(r));
}
#define VK(call) check((call), #call)
constexpr unsigned screen_w = 1280, screen_h = 720, tex_w = 1024, tex_h = 512;
struct Vertex { float x,y,u,v; };
}
struct PS4Video::Impl {
    VkInstance instance{}; VkPhysicalDevice physical{}; VkDevice device{}; VkQueue queue{};
    VkSwapchainKHR swap{}; std::vector<VkImage> images;
    std::vector<VkImageView> views; std::vector<VkFramebuffer> targets;
    VkRenderPass pass{}; VkPipeline pipeline{}; VkPipelineLayout pipeline_layout{};
    VkImage texture{}; VkImageView texture_view{}; VkDeviceMemory texture_memory{};
    VkSubresourceLayout texture_layout{}; void* texture_map{};
    VkBuffer vertices{}; VkDeviceMemory vertex_memory{}; void* vertex_map{};
    VkSampler sampler{}; VkDescriptorSetLayout set_layout{}; VkDescriptorPool descriptors{}; VkDescriptorSet set{};
    VkCommandPool commands{}; VkCommandBuffer command{}; VkFence fence{};
    bool first = true, pending = false, benchmark=false;
    VkDeviceMemory benchmark_memory{};
    uint32_t pending_index=0;
    PS4VideoTiming timing;
    uint32_t memory_type(uint32_t bits) {
        VkPhysicalDeviceMemoryProperties props{}; vkGetPhysicalDeviceMemoryProperties(physical, &props);
        for (unsigned i = 0; i < props.memoryTypeCount; ++i)
            if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) return i;
        throw std::runtime_error("No host-visible Vulkan memory");
    }
    VkDeviceMemory allocate(VkMemoryRequirements req) {
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        a.allocationSize = req.size; a.memoryTypeIndex = memory_type(req.memoryTypeBits);
        VkDeviceMemory m; VK(vkAllocateMemory(device, &a, nullptr, &m)); return m;
    }
    VkImageView view(VkImage image, VkFormat format) {
        VkImageViewCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        i.image = image; i.viewType = VK_IMAGE_VIEW_TYPE_2D; i.format = format;
        i.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView v; VK(vkCreateImageView(device, &i, nullptr, &v)); return v;
    }
    void init() {
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        VK(vkCreateInstance(&ici, nullptr, &instance));
        uint32_t count = 1; VK(vkEnumeratePhysicalDevices(instance, &count, &physical));
        if (!count) throw std::runtime_error("No Vulkan device");
        float priority = 1;
        VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        q.queueCount = 1; q.pQueuePriorities = &priority;
        const char* extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        d.queueCreateInfoCount = 1; d.pQueueCreateInfos = &q;
        d.enabledExtensionCount = 1; d.ppEnabledExtensionNames = &extension;
        VK(vkCreateDevice(physical, &d, nullptr, &device)); vkGetDeviceQueue(device, 0, 0, &queue);
        VkSwapchainCreateInfoKHR s{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        s.minImageCount = 4; s.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
        s.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; s.imageExtent = {screen_w, screen_h};
        s.imageArrayLayers = 1; s.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        s.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        s.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; s.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        VK(vkCreateSwapchainKHR(device, &s, nullptr, &swap));
        if(benchmark) {
            // A private render target: never render into an image being scanned
            // out, and never change VideoOut refresh rate to inflate throughput.
            VkImageCreateInfo target{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            target.imageType=VK_IMAGE_TYPE_2D; target.format=s.imageFormat;
            target.extent={screen_w,screen_h,1}; target.mipLevels=target.arrayLayers=1;
            target.samples=VK_SAMPLE_COUNT_1_BIT; target.tiling=VK_IMAGE_TILING_OPTIMAL;
            target.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            VkImage image; VK(vkCreateImage(device,&target,nullptr,&image));
            VkMemoryRequirements req{}; vkGetImageMemoryRequirements(device,image,&req);
            VkPhysicalDeviceMemoryProperties props{}; vkGetPhysicalDeviceMemoryProperties(physical,&props);
            uint32_t type=UINT32_MAX;
            for(uint32_t i=0;i<props.memoryTypeCount;++i)
                if((req.memoryTypeBits&(1u<<i)) && (props.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) { type=i; break; }
            if(type==UINT32_MAX) throw std::runtime_error("No device-local benchmark target memory");
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            alloc.allocationSize=req.size; alloc.memoryTypeIndex=type;
            VK(vkAllocateMemory(device,&alloc,nullptr,&benchmark_memory));
            VK(vkBindImageMemory(device,image,benchmark_memory,0)); images.push_back(image);
        } else {
            VK(vkGetSwapchainImagesKHR(device, swap, &count, nullptr)); images.resize(count);
            VK(vkGetSwapchainImagesKHR(device, swap, &count, images.data()));
        }
        VkAttachmentDescription a{}; a.format = s.imageFormat; a.samples = VK_SAMPLE_COUNT_1_BIT;
        a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; a.finalLayout = benchmark?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1; sub.pColorAttachments = &color;
        VkSubpassDependency dependency{}; dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0; dependency.srcStageMask = dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rp.attachmentCount = 1; rp.pAttachments = &a; rp.subpassCount = 1; rp.pSubpasses = &sub;
        rp.dependencyCount = 1; rp.pDependencies = &dependency;
        VK(vkCreateRenderPass(device, &rp, nullptr, &pass));
        for (auto image : images) {
            views.push_back(view(image, s.imageFormat));
            VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fb.renderPass = pass; fb.attachmentCount = 1; fb.pAttachments = &views.back();
            fb.width = screen_w; fb.height = screen_h; fb.layers = 1;
            VkFramebuffer target; VK(vkCreateFramebuffer(device, &fb, nullptr, &target)); targets.push_back(target);
        }
        VkImageCreateInfo t{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        t.imageType = VK_IMAGE_TYPE_2D; t.format = VK_FORMAT_R8G8B8A8_SRGB; t.extent = {tex_w,tex_h,1};
        t.mipLevels = t.arrayLayers = 1; t.samples = VK_SAMPLE_COUNT_1_BIT; t.tiling = VK_IMAGE_TILING_LINEAR;
        t.usage = VK_IMAGE_USAGE_SAMPLED_BIT; t.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        VK(vkCreateImage(device, &t, nullptr, &texture));
        VkMemoryRequirements req{}; vkGetImageMemoryRequirements(device, texture, &req);
        texture_memory = allocate(req); VK(vkBindImageMemory(device, texture, texture_memory, 0));
        VK(vkMapMemory(device, texture_memory, 0, VK_WHOLE_SIZE, 0, &texture_map));
        VkImageSubresource resource{VK_IMAGE_ASPECT_COLOR_BIT,0,0};
        vkGetImageSubresourceLayout(device, texture, &resource, &texture_layout);
        if (texture_layout.rowPitch < tex_w * 4 || texture_layout.offset + texture_layout.rowPitch * tex_h > req.size)
            throw std::runtime_error("Invalid linear texture layout");
        std::memset(texture_map, 0, static_cast<size_t>(req.size));
        texture_view = view(texture, t.format);
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; b.size = sizeof(Vertex)*6; b.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VK(vkCreateBuffer(device, &b, nullptr, &vertices)); vkGetBufferMemoryRequirements(device, vertices, &req);
        vertex_memory = allocate(req); VK(vkBindBufferMemory(device, vertices, vertex_memory, 0));
        VK(vkMapMemory(device, vertex_memory, 0, VK_WHOLE_SIZE, 0, &vertex_map));
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = si.minFilter = VK_FILTER_NEAREST; si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.maxLod = 0;
        VK(vkCreateSampler(device, &si, nullptr, &sampler));
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        sl.bindingCount = 1; sl.pBindings = &binding; VK(vkCreateDescriptorSetLayout(device,&sl,nullptr,&set_layout));
        VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1};
        VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dp.maxSets = 1; dp.poolSizeCount = 1; dp.pPoolSizes = &pool_size; VK(vkCreateDescriptorPool(device,&dp,nullptr,&descriptors));
        VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ds.descriptorPool = descriptors; ds.descriptorSetCount = 1; ds.pSetLayouts = &set_layout;
        VK(vkAllocateDescriptorSets(device,&ds,&set));
        VkDescriptorImageInfo image_info{sampler, texture_view, VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set; write.descriptorCount = 1; write.descriptorType = binding.descriptorType; write.pImageInfo = &image_info;
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; pl.setLayoutCount=1; pl.pSetLayouts=&set_layout;
        VK(vkCreatePipelineLayout(device,&pl,nullptr,&pipeline_layout));
        VkShaderModule modules[2]{};
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize=sizeof(frame_vert); sm.pCode=frame_vert; VK(vkCreateShaderModule(device,&sm,nullptr,&modules[0]));
        sm.codeSize=sizeof(frame_frag); sm.pCode=frame_frag; VK(vkCreateShaderModule(device,&sm,nullptr,&modules[1]));
        VkPipelineShaderStageCreateInfo stages[2]{};
        for(int i=0;i<2;++i) { stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[i].module=modules[i]; stages[i].pName="main"; }
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
        VkVertexInputBindingDescription vb{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attrs[2]={{0,0,VK_FORMAT_R32G32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,8}};
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount=1; vi.pVertexBindingDescriptions=&vb; vi.vertexAttributeDescriptionCount=2; vi.pVertexAttributeDescriptions=attrs;
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,float(screen_w),float(screen_h),0,1}; VkRect2D scissor{{0,0},{screen_w,screen_h}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount=vp.scissorCount=1; vp.pViewports=&viewport; vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.polygonMode=VK_POLYGON_MODE_FILL; rs.lineWidth=1; rs.cullMode=VK_CULL_MODE_NONE;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{}; blend.colorWriteMask=0xf;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cb.attachmentCount=1; cb.pAttachments=&blend;
        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        gp.stageCount=2; gp.pStages=stages; gp.pVertexInputState=&vi; gp.pInputAssemblyState=&ia; gp.pViewportState=&vp;
        gp.pRasterizationState=&rs; gp.pMultisampleState=&ms; gp.pColorBlendState=&cb; gp.layout=pipeline_layout; gp.renderPass=pass;
        VK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&gp,nullptr,&pipeline));
        for(auto module:modules) vkDestroyShaderModule(device,module,nullptr);
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK(vkCreateCommandPool(device,&cp,nullptr,&commands));
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ca.commandPool=commands; ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ca.commandBufferCount=1;
        VK(vkAllocateCommandBuffers(device,&ca,&command));
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VK(vkCreateFence(device,&fi,nullptr,&fence));
    }
    void present(const uint32_t* bgra, int w, int h) {
        uint64_t work_start=sceKernelGetProcessTime(), flip_us=0, gpu_wait_us=0;
        // The CPU emulates the next frame while the preceding draw runs. Wait
        // before reusing its texture, vertices, command buffer or EOP packet.
        if(pending) {
            VK(vkWaitForFences(device,1,&fence,VK_TRUE,2000000000ull));
            gpu_wait_us=sceKernelGetProcessTime()-work_start;
            VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            pi.swapchainCount=1; pi.pSwapchains=&swap; pi.pImageIndices=&pending_index;
            uint64_t flip_start=sceKernelGetProcessTime();
            VK(vkQueuePresentKHR(queue,&pi));
            ps4_check_present();
            flip_us=sceKernelGetProcessTime()-flip_start;
            pending=false;
        }
        uint64_t upload_start=sceKernelGetProcessTime();
        if(!bgra || w<=0 || w>int(tex_w) || h<=0 || h>int(tex_h)) throw std::runtime_error("Invalid frame size");
        for(int y=0;y<h;++y) {
            auto* dst=reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(texture_map)+texture_layout.offset+y*texture_layout.rowPitch);
            for(int x=0;x<w;++x) { uint32_t c=bgra[y*w+x]; dst[x]=(c&0xff00ff00u)|((c&255)<<16)|((c>>16)&255); }
        }
        float scale=std::min(float(screen_w)/w,float(screen_h)/h);
        float x=w*scale/screen_w,y=h*scale/screen_h,u=float(w)/tex_w,v=float(h)/tex_h;
        const Vertex data[6]={{-x,-y,0,0},{x,-y,u,0},{x,y,u,v},{-x,-y,0,0},{x,y,u,v},{-x,y,0,v}};
        std::memcpy(vertex_map,data,sizeof(data));
        VkMappedMemoryRange ranges[2]{};
        for(auto& r:ranges) { r.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE; r.size=VK_WHOLE_SIZE; }
        ranges[0].memory=texture_memory; ranges[1].memory=vertex_memory;
        VK(vkFlushMappedMemoryRanges(device,2,ranges));
        uint64_t upload_us=sceKernelGetProcessTime()-upload_start;
        uint64_t acquire_start=sceKernelGetProcessTime();
        // Serialize acquisition, upload and presentation for this first backend.
        // One fence avoids the ICD's shared semaphore epilogue-buffer reuse path.
        uint32_t index=0;
        if(!benchmark) {
            VK(vkResetFences(device,1,&fence));
            VK(vkAcquireNextImageKHR(device,swap,2000000000ull,VK_NULL_HANDLE,fence,&index));
            VK(vkWaitForFences(device,1,&fence,VK_TRUE,2000000000ull));
        }
        if(index>=targets.size()) throw std::runtime_error("Invalid swapchain index");
        uint64_t retire_start=sceKernelGetProcessTime();
        uint64_t acquire_us=retire_start-acquire_start;
        if(!benchmark) ps4_wait_image_reusable(index);
        uint64_t retire_us=sceKernelGetProcessTime()-retire_start;
        uint64_t record_start=sceKernelGetProcessTime();
        VK(vkResetCommandBuffer(command,0));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK(vkBeginCommandBuffer(command,&begin));
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout=first?VK_IMAGE_LAYOUT_PREINITIALIZED:VK_IMAGE_LAYOUT_GENERAL; barrier.newLayout=VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.image=texture; barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        VkMemoryBarrier vb{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; vb.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT; vb.dstAccessMask=VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,1,&vb,0,nullptr,1,&barrier);
        VkClearValue black{}; black.color.float32[3]=1;
        VkRenderPassBeginInfo rb{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; rb.renderPass=pass; rb.framebuffer=targets[index]; rb.renderArea.extent={screen_w,screen_h}; rb.clearValueCount=1; rb.pClearValues=&black;
        vkCmdBeginRenderPass(command,&rb,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&set,0,nullptr);
        VkDeviceSize offset=0; vkCmdBindVertexBuffers(command,0,1,&vertices,&offset);
        vkCmdDraw(command,6,1,0,0); vkCmdEndRenderPass(command); VK(vkEndCommandBuffer(command));
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount=1; submit.pCommandBuffers=&command;
        uint64_t record_us=sceKernelGetProcessTime()-record_start;
        uint64_t submit_start=sceKernelGetProcessTime();
        VK(vkResetFences(device,1,&fence)); VK(vkQueueSubmit(queue,1,&submit,fence));
        uint64_t submit_us=sceKernelGetProcessTime()-submit_start;
        if(benchmark) {
            uint64_t wait_start=sceKernelGetProcessTime();
            VK(vkWaitForFences(device,1,&fence,VK_TRUE,2000000000ull));
            gpu_wait_us=sceKernelGetProcessTime()-wait_start;
        } else { pending=true; pending_index=index; }
        timing={sceKernelGetProcessTime()-work_start-retire_us-flip_us,flip_us+retire_us,
                gpu_wait_us,upload_us,acquire_us,record_us,submit_us};
        first=false;
    }
    ~Impl() {
        // Process-exit ownership for bring-up: do not attempt cleanup after a lost GPU.
        // All resources are bounded and allocated once; the OS reclaims them on exit.
    }
};
PS4Video::PS4Video(bool benchmark):p(new Impl) { p->benchmark=benchmark; p->init(); }
PS4Video::~PS4Video() = default;
void PS4Video::present(const uint32_t* pixels,int w,int h) { p->present(pixels,w,h); }
PS4VideoTiming PS4Video::timing() const { return p->timing; }
