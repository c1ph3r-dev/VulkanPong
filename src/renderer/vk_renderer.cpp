#include "vk_renderer.hpp"

#include <windows.h>
#include <vulkan/vulkan_win32.h>

#include "vk_shader_util.hpp"
#include "dds_structs.hpp"
#include "vk_init.hpp"
#include "vk_utils.hpp"
#include "shared_render_types.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageFlags,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    JONO_ASSERT(0, pCallbackData->pMessage);
    return false;
}

Image* VkRenderer::vk_create_image(AssetTypeID id)
{
    Image* image = nullptr;

    if(context->image_count < MAX_IMAGES)
    {
        // TODO: Suballocation from Main Allocation
        const char* data = get_asset(id);
        uint32_t width = 1, height = 1;
        const char* data_begin = data;

        if(id != ASSET_SPRITE_WHITE)
        {
            const DDS_FILE* file = (const DDS_FILE*)data;
            width = file->header.Width;
            height = file->header.Height;
            data_begin = &file->data_begin;
        }

        uint32_t texture_size = width * height * 4u;

        vk_copy_to_buffer(&context->staging_buffer, data_begin, texture_size);

        image = &context->images[context->image_count++];
        *image = vk_allocate_image(context->device, context->gpu, width, height, VK_FORMAT_R8G8B8A8_UNORM);
        
        image->id = id;

        if(image->image && image->memory)
        {
            VkCommandBuffer cmd;
            auto cmd_alloc = cmd_alloc_info(context->command_pool);
            VK_CHECK(vkAllocateCommandBuffers(context->device, &cmd_alloc, &cmd));

            VkCommandBufferBeginInfo begin_info = cmd_begin_info();
            vkBeginCommandBuffer(cmd, &begin_info);

            VkImageSubresourceRange range = {};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.layerCount = 1;
            range.levelCount = 1;

            // Transition layout to transfer optimal
            VkImageMemoryBarrier img_mem_barrier = {};
            img_mem_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            img_mem_barrier.image = image->image;
            img_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            img_mem_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            img_mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            img_mem_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            img_mem_barrier.subresourceRange = range;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &img_mem_barrier);


            VkBufferImageCopy copy_region = {};
            copy_region.imageExtent = {width, height, 1u};
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            vkCmdCopyBufferToImage(cmd, context->staging_buffer.buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            img_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            img_mem_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            img_mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            img_mem_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &img_mem_barrier);

            VK_CHECK(vkEndCommandBuffer(cmd));

            VkFence upload_fence;
            auto f_info = fence_info();
            VK_CHECK(vkCreateFence(context->device, &f_info, VK_NULL_HANDLE, &upload_fence));

            // Create image view
            {
                VkImageViewCreateInfo img_info = {};
                img_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                img_info.image = image->image;
                img_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                img_info.format = VK_FORMAT_R8G8B8A8_UNORM;
                img_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                img_info.subresourceRange.layerCount = 1;
                img_info.subresourceRange.levelCount = 1;


                VK_CHECK(vkCreateImageView(context->device, &img_info, VK_NULL_HANDLE, &image->view));
            }

            auto s_info = submit_info(&cmd);
            VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &s_info, upload_fence));

            VK_CHECK(vkWaitForFences(context->device, 1, &upload_fence, true, UINT64_MAX));
        }
        else
        {
            JONO_ASSERT(0, "Failed to allocate Image for AssetTypeID: %d", id);
            JONO_ERROR("Failed to allocate Image for AssetTypeID: %d", id);
            // Reset allocated image in memory
            context->image_count--;
            image = nullptr;
        }

        // Free Allocated Asset Data
        delete data;
    }
    else
    {
        JONO_ASSERT(0, "Reached Maximum amount of images");
    }

    return image;
}

Image* VkRenderer::vk_get_image(AssetTypeID id)
{
    Image* image = nullptr;

    for (uint32_t i = 0; i < context->image_count; i++)
    {
        Image* img = &context->images[i];
        if(img->id == id)
        {
            image = img;
            break;
        }
    }
    
    if(!image)
    {
        image = vk_create_image(id);
    }

    return image;
}

Descriptor* VkRenderer::vk_create_descriptor(AssetTypeID id)
{
    Descriptor* descriptor = nullptr;
    
    if(context->descriptor_count < MAX_DESCRIPTORS)
    {
        descriptor = &context->descriptors[context->descriptor_count++];
        *descriptor = {};
        descriptor->id = id;

        create_descriptor_set(descriptor);

        if(descriptor->descriptor_set)
        {
            update_descriptor_set(descriptor);
        }
        else
        {
            descriptor = nullptr;
            context->descriptor_count--;
            JONO_ASSERT(0, "Failed to allocate Descriptor Set for AssetTypeID: %d", id);
            JONO_ERROR("Failed to allocate Descriptor Set for AssetTypeID: %d", id);
        }        
    }
    else
    {
        JONO_ASSERT(0, "Reached Maximum amount of descriptors");
    }

    return descriptor;
}

Descriptor* VkRenderer::vk_get_descriptor(AssetTypeID id)
{
    Descriptor* descriptor = nullptr;

    for(uint32_t i = 0; i < context->descriptor_count; i++)
    {
        Descriptor* desc = &context->descriptors[i];
        if(desc->id == id)
        {
            descriptor = desc;
            break;
        }
    }

    if(!descriptor)
    {
        descriptor = vk_create_descriptor(id);
    }

    return descriptor;
}

RenderCommand* VkRenderer::vk_add_render_command(Descriptor* descriptor)
{
    JONO_ASSERT(descriptor, "Invalid Descriptor provided");

    RenderCommand* rc = nullptr;

    if(context->render_command_count < MAX_RENDER_COMMANDS)
    {
        rc = &context->render_commands[context->render_command_count++];
        *rc = {};
        rc->descriptor = descriptor;
        rc->push_data.transformIdx = context->transform_count;
    }
    else
    {
        JONO_ASSERT(0, "Reached Maximum amount of render commands");
        JONO_ERROR("Reached Maximum amount of render commands");
    }

    return rc;
}

bool VkRenderer::create_debugger()
{
    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");

    if(vkCreateDebugUtilsMessengerEXT)
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_info = {};
        debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debug_info.pfnUserCallback = debug_callback;

        vkCreateDebugUtilsMessengerEXT(context->instance, &debug_info, VK_NULL_HANDLE, &context->debug_messenger);
    }
    else
    {
        return false;
    }

    return true;
}

void VkRenderer::create_instance()
{
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Pong";
    app_info.pEngineName = "Ponggine";

    const char* extensions[] = {
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
    };

    const char* layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.ppEnabledExtensionNames = extensions;
    instance_info.enabledExtensionCount = ArraySize(extensions);
    instance_info.ppEnabledLayerNames = layers;
    instance_info.enabledLayerCount = ArraySize(layers);
    VK_CHECK(vkCreateInstance(&instance_info, VK_NULL_HANDLE, &context->instance));
}

void VkRenderer::create_window(void* window)
{
    VkWin32SurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hwnd = (HWND)window;
    surface_info.hinstance = GetModuleHandleA(0);
    VK_CHECK(vkCreateWin32SurfaceKHR(context->instance, &surface_info, VK_NULL_HANDLE, &context->surface));
}

bool VkRenderer::select_gpu()
{
    context->graphics_idx = -1;
    uint32_t gpu_count = 0;
    VkPhysicalDevice gpus[10];
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &gpu_count, 0));
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &gpu_count, gpus));

    for(uint32_t i = 0; i < 1; i++) // 1 because Integrated graphics have better support, otherwise use gpu count
    {
        VkPhysicalDevice gpu = gpus[i];

        uint32_t queue_family_count = 0;
        VkQueueFamilyProperties queue_properties[10];

        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, 0);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, queue_properties);

        for(uint32_t j = 0; j < queue_family_count; j++)
        {
            if(queue_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                VkBool32 surface_support = VK_FALSE;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, context->surface, &surface_support));

                if(surface_support)
                {
                    context->graphics_idx = j;
                    context->gpu = gpu;
                    break;
                }
            }
        }
    }

    if(context->graphics_idx < 0) return false;

    return true;
}

void VkRenderer::create_device()
{
    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = context->graphics_idx;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.queueCreateInfoCount = 1;
    device_info.ppEnabledExtensionNames = extensions;
    device_info.enabledExtensionCount = ArraySize(extensions);

    VK_CHECK(vkCreateDevice(context->gpu, &device_info, VK_NULL_HANDLE, &context->device));

    vkGetDeviceQueue(context->device, context->graphics_idx, 0, &context->graphics_queue);
}

void VkRenderer::create_image_views()
{
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.format = context->surface_format.format;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.layerCount = 1;
    view_info.subresourceRange.levelCount = 1;

    for (uint32_t i = 0; i < context->swapchain_image_count; i++)
    {
        view_info.image = context->swapchain_images[i];
        VK_CHECK(vkCreateImageView(context->device, &view_info, VK_NULL_HANDLE, &context->swapchain_image_views[i]));
    }
}

void VkRenderer::create_swapchain()
{
    uint32_t format_count = 0;
    VkSurfaceFormatKHR surface_formats[10];
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->gpu, context->surface, &format_count, 0));
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->gpu, context->surface, &format_count, surface_formats));

    for(auto& format : surface_formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            context->surface_format = format;
            break;
        }
    }

    VkSurfaceCapabilitiesKHR surface_caps = {};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->gpu, context->surface, &surface_caps));
    uint32_t image_count = surface_caps.minImageCount;
    image_count = image_count > surface_caps.maxImageCount ? image_count-1 : image_count;

    VkSwapchainCreateInfoKHR swapchain_info = {};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.surface = context->surface;
    swapchain_info.imageFormat = context->surface_format.format;
    swapchain_info.preTransform = surface_caps.currentTransform;
    swapchain_info.imageExtent = surface_caps.currentExtent;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageArrayLayers = 1;

    VK_CHECK(vkCreateSwapchainKHR(context->device, &swapchain_info, 0, &context->swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->swapchain_image_count, 0));
    VK_CHECK(vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->swapchain_image_count, context->swapchain_images));

    create_image_views();
}

void VkRenderer::create_render_pass()
{
    VkAttachmentDescription attachment = {};
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.format = context->surface_format.format;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.colorAttachmentCount = 1;

    VkAttachmentDescription attachments[] = {
        attachment
    };

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.pAttachments = attachments;
    render_pass_info.attachmentCount = ArraySize(attachments);
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.subpassCount = 1;

    VK_CHECK(vkCreateRenderPass(context->device, &render_pass_info, VK_NULL_HANDLE, &context->render_pass));
}

void VkRenderer::create_framebuffers()
{
    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = context->render_pass;
    framebuffer_info.width = context->screen_size.width;
    framebuffer_info.height = context->screen_size.height;
    framebuffer_info.layers = 1;
    framebuffer_info.attachmentCount = 1;

    for(uint32_t i = 0; i < context->swapchain_image_count; i++)
    {
        framebuffer_info.pAttachments = &context->swapchain_image_views[i];
        VK_CHECK(vkCreateFramebuffer(context->device, &framebuffer_info, VK_NULL_HANDLE, &context->framebuffers[i]));
    }
}

void VkRenderer::create_descriptor_set_layouts()
{
    VkDescriptorSetLayoutBinding layout_bindings[] = {
        layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, 0),
        layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, 1),
        layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 2),
        layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 3)
    };

    VkDescriptorSetLayoutCreateInfo dsl_info = {};
    dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_info.pBindings = layout_bindings;
    dsl_info.bindingCount = ArraySize(layout_bindings);

    VK_CHECK(vkCreateDescriptorSetLayout(context->device, &dsl_info, VK_NULL_HANDLE, &context->ds_layout));
}

void VkRenderer::create_pipeline_layout()
{
    VkPushConstantRange pc = {};
    pc.size = sizeof(PushData);
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &context->ds_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &pc;
    VK_CHECK(vkCreatePipelineLayout(context->device, &pipeline_layout_info, VK_NULL_HANDLE, &context->pipeline_layout));
}

void VkRenderer::create_pipeline()
{
    VkShaderModule vertex_shader, fragment_shader;

        uint32_t size_in_bytes;
        char* code = platform_read_file((char*)"assets/shaders/compiled/shader.vert.spv", &size_in_bytes);

        VkShaderModuleCreateInfo shader_info = {};
        shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_info.pCode = (uint32_t *)code;
        shader_info.codeSize = size_in_bytes;
        VK_CHECK(vkCreateShaderModule(context->device, &shader_info, VK_NULL_HANDLE, &vertex_shader));
        delete code;

        code = platform_read_file((char*)"assets/shaders/compiled/shader.frag.spv", &size_in_bytes);
        shader_info.pCode = (uint32_t *)code;
        shader_info.codeSize = size_in_bytes;
        VK_CHECK(vkCreateShaderModule(context->device, &shader_info, VK_NULL_HANDLE, &fragment_shader));
        delete code;

        VkPipelineShaderStageCreateInfo vertex_stage = {};
        vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_stage.pName = "main";
        vertex_stage.module = vertex_shader;

        VkPipelineShaderStageCreateInfo fragment_stage = {};
        fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_stage.pName = "main";
        fragment_stage.module = fragment_shader;

        VkPipelineShaderStageCreateInfo shader_stages[] = {
            vertex_stage, fragment_stage
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState color_attachment = {};
        color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blend_state = {};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.pAttachments = &color_attachment;
        color_blend_state.attachmentCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization_state = {};
        rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_state.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample_state = {};
        multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkRect2D scissor = {};
        VkViewport viewport = {};
        viewport.maxDepth = 1.0f;

        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.pDynamicStates = dynamic_states;
        dynamic_state.dynamicStateCount = ArraySize(dynamic_states);

        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.layout = context->pipeline_layout;
        pipeline_info.renderPass = context->render_pass;
        pipeline_info.pVertexInputState = &vertex_input_state;
        pipeline_info.pColorBlendState = &color_blend_state;
        pipeline_info.pStages = shader_stages;
        pipeline_info.stageCount = ArraySize(shader_stages);
        pipeline_info.pRasterizationState = &rasterization_state;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.pMultisampleState = &multisample_state;
        pipeline_info.pInputAssemblyState = &input_assembly;

        VK_CHECK(vkCreateGraphicsPipelines(context->device, 0, 1, &pipeline_info, VK_NULL_HANDLE, &context->pipeline));

        vkDestroyShaderModule(context->device, vertex_shader, 0);
        vkDestroyShaderModule(context->device, fragment_shader, 0);
}

void VkRenderer::create_command_pool()
{
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = context->graphics_idx;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(context->device, &pool_info, VK_NULL_HANDLE, &context->command_pool));

    // Allocate Command Buffers
    auto alloc_info = cmd_alloc_info(context->command_pool);
    VK_CHECK(vkAllocateCommandBuffers(context->device, &alloc_info, &context->cmd));
}

void VkRenderer::create_sync_objects()
{
    // Semaphores
    VkSemaphoreCreateInfo sema_info = {};
    sema_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK(vkCreateSemaphore(context->device, &sema_info, VK_NULL_HANDLE, &context->aquire_semaphore));
    VK_CHECK(vkCreateSemaphore(context->device, &sema_info, VK_NULL_HANDLE, &context->submit_semaphore));

    // Fence
    auto info = fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(context->device, &info, VK_NULL_HANDLE, &context->fence));
}

void VkRenderer::create_buffers()
{
    // Staging Buffer
    context->staging_buffer = vk_allocate_buffer(
        context->device, 
        context->gpu, 
        MB(1), 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // Transform Storage Buffer
    context->transform_storage_buffer = vk_allocate_buffer(
        context->device, 
        context->gpu, 
        sizeof(Transform) * MAX_ENTITIES, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // Material Storage Buffer
    context->material_storage_buffer = vk_allocate_buffer(
        context->device,
        context->gpu,
        sizeof(MaterialData) * MAX_MATERIALS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // Global Uniform Buffer Object
    context->global_UBO = vk_allocate_buffer(
        context->device,
        context->gpu,
        sizeof(GlobalData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    GlobalData global_data = {
        (int)context->screen_size.width,
        (int)context->screen_size.height};

    vk_copy_to_buffer(&context->global_UBO, &global_data, sizeof(global_data));

    // Create Index Buffer
    context->index_buffer = vk_allocate_buffer(
        context->device,
        context->gpu,
        sizeof(uint32_t) * 6,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Copy Indices to the buffer
    {
        uint32_t indices[] = {
            0, 1, 2, 2, 3, 0
        };

        vk_copy_to_buffer(&context->index_buffer, indices, sizeof(indices));
    }
}

void VkRenderer::create_sampler()
{
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.magFilter = VK_FILTER_NEAREST;

    VK_CHECK(vkCreateSampler(context->device, &sampler_info, VK_NULL_HANDLE, &context->sampler));
}

void VkRenderer::create_descriptor_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = MAX_DESCRIPTORS;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.poolSizeCount = ArraySize(pool_sizes);

    VK_CHECK(vkCreateDescriptorPool(context->device, &pool_info, VK_NULL_HANDLE, &context->descriptor_pool));
}

void VkRenderer::create_descriptor_set(Descriptor* descriptor)
{
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pSetLayouts = &context->ds_layout;
    alloc_info.descriptorSetCount = 1;
    alloc_info.descriptorPool = context->descriptor_pool;

    VK_CHECK(vkAllocateDescriptorSets(context->device, &alloc_info, &descriptor->descriptor_set));
}

void VkRenderer::update_descriptor_set(Descriptor* descriptor)
{
    Image* image = vk_get_image(descriptor->id);
    DescriptorInfo desc_info[] = {
        DescriptorInfo(context->global_UBO.buffer),
        DescriptorInfo(context->transform_storage_buffer.buffer),
        DescriptorInfo(context->sampler, image->view),
        DescriptorInfo(context->material_storage_buffer.buffer)
    };

    VkWriteDescriptorSet writes[] = {
        write_set(descriptor->descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc_info[0], 0, 1),
        write_set(descriptor->descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &desc_info[1], 1, 1),
        write_set(descriptor->descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc_info[2], 2, 1),
        write_set(descriptor->descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &desc_info[3], 3, 1)
    };

    vkUpdateDescriptorSets(context->device, ArraySize(writes), writes, 0, VK_NULL_HANDLE);
}

bool VkRenderer::init(void* window)
{
    vk_compile_shader("assets/shaders/shader.vert", "assets/shaders/compiled/shader.vert.spv");
    vk_compile_shader("assets/shaders/shader.frag", "assets/shaders/compiled/shader.frag.spv");

    platform_get_window_size(&context->screen_size.width, &context->screen_size.height);

    create_instance();

    if(!create_debugger())
    {
        JONO_ASSERT(0, "Failed to create Vulkan debugger");
        JONO_ERROR("Failed to create Vulkan debugger");
        return false;
    }

    create_window(window);

    if(!select_gpu()) 
    {
        JONO_ASSERT(0, "Could not find a GPU");
        JONO_FATAL("Could not find a GPU");
        return false;
    }

    create_device();
    create_swapchain();
    create_render_pass();
    create_framebuffers();
    create_descriptor_set_layouts();
    create_pipeline_layout();
    create_pipeline();
    create_command_pool();
    create_sync_objects();
    create_buffers();

    // Create Image
    Image* image = vk_create_image(ASSET_SPRITE_WHITE);
    if(!image->image || !image->view || !image->memory)
    {
        JONO_ASSERT(0, "Failed to create Default Texture");
        JONO_FATAL("Failed to create Default Texture");
        return false;
    }

    create_sampler();
    create_descriptor_pool();

    return true;
}

void VkRenderer::int_render(const VkCommandBuffer& cmd, GameState* state)
{
    // Rendering Commands
    for(uint32_t i = 0; i < context->render_command_count; i++)
    {
        RenderCommand* rc = &context->render_commands[i];

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline_layout, 0, 1, &rc->descriptor->descriptor_set, 0, 0);
        vkCmdPushConstants(cmd, context->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushData), &rc->push_data);
        vkCmdDrawIndexed(cmd, 6, rc->instance_count, 0, 0, 0);
    }

    // Reset the render commands for the next frame
    context->render_command_count = 0;
}

bool VkRenderer::render(GameState* state)
{
    uint32_t img_idx;

    // We wait on the GPU to be done with the work
    VK_CHECK(vkWaitForFences(context->device, 1, &context->fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(context->device, 1, &context->fence));

    // Entity rendering
    {
        for(uint32_t i = 0; i < state->entity_count; i++)
        {
            Entity* e = &state->entities[i];

            Material* m  = get_material(state, e->pos.materialIdx);

            auto desc = vk_get_descriptor(m->id);
            if(desc)
            {
                auto rc = vk_add_render_command(desc);
                if(rc)
                {
                    rc->instance_count = 1;
                }    
            }

            context->transforms[context->transform_count++] = e->pos;
        }
    }

    // Copy data to buffers
    {
        vk_copy_to_buffer(&context->transform_storage_buffer, &context->transforms, sizeof(Transform) * context->transform_count);
        context->transform_count = 0;
        
        MaterialData material_data[MAX_MATERIALS];
        for(uint32_t i = 0; i < state->material_count; i++)
        {
            material_data[i] = state->materials[i].data;
        }
        vk_copy_to_buffer(&context->material_storage_buffer, material_data, sizeof(MaterialData) * state->material_count);
    }

    // This waits on the timeout until the image is ready, if timeout reached -> VK_TIMEOUT
    VK_CHECK(vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX, context->aquire_semaphore, VK_NULL_HANDLE, &img_idx));

    VkCommandBuffer cmd = context->cmd;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    VkClearValue clear_value = {};
    clear_value.color = {1.0f, 0.8f, 0.3f, 1.0f};

    VkRenderPassBeginInfo rp_begin_info = {};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = context->render_pass;
    rp_begin_info.renderArea.extent = context->screen_size;
    rp_begin_info.framebuffer = context->framebuffers[img_idx];
    rp_begin_info.pClearValues = &clear_value;
    rp_begin_info.clearValueCount = 1;
    vkCmdBeginRenderPass(cmd, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.width = context->screen_size.width;
    viewport.height =context->screen_size.height;
    viewport.maxDepth = 1.0f;
        
    VkRect2D scissor = {};
    scissor.extent = context->screen_size;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindIndexBuffer(cmd, context->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline);

    // Rendering Commands
    int_render(cmd, state);

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    auto s_info = submit_info(&cmd);
    s_info.pWaitDstStageMask = &wait_stage;
    s_info.pSignalSemaphores = &context->submit_semaphore;
    s_info.signalSemaphoreCount = 1;
    s_info.pWaitSemaphores = &context->aquire_semaphore;
    s_info.waitSemaphoreCount = 1;
    VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &s_info, context->fence));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pSwapchains = &context->swapchain;
    present_info.swapchainCount = 1;
    present_info.pImageIndices = &img_idx;
    present_info.pWaitSemaphores = &context->submit_semaphore;
    present_info.waitSemaphoreCount = 1;
    vkQueuePresentKHR(context->graphics_queue, &present_info); // TODO: VK_CHECK

    return true;
}
