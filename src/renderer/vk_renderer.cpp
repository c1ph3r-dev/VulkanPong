#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "dds_structs.hpp"
#include "logger.hpp"

#include "vk_utils.cpp"
#include "vk_init.hpp"

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

struct VkContext
{
    VkExtent2D screen_size;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_format;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphics_queue;
    VkSwapchainKHR swapchain;
    VkDescriptorSetLayout ds_layout;
    VkDescriptorSet descriptor_set;
    VkSampler sampler;
    VkDescriptorPool descriptor_pool;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer cmd;
    VkRenderPass render_pass;
    Buffer staging_buffer;

    VkSemaphore aquire_semaphore;
    VkSemaphore submit_semaphore;

    VkFence fence;

    uint32_t swapchain_image_count;
    //TODO: Suballocation from Main Allocation
    VkImage swapchain_images[5];
    VkImageView swapchain_image_views[5];
    VkFramebuffer framebuffers[5];

    //TODO: Will be abstracted
    Image image;

    int graphics_idx;
};

bool vk_init(VkContext* context, void* window) {
    platform_get_window_size(&context->screen_size.width, &context->screen_size.height);

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

    // Create Surface
    {
        VkWin32SurfaceCreateInfoKHR surface_info = {};
        surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surface_info.hwnd = (HWND)window;
        surface_info.hinstance = GetModuleHandleA(0);
        VK_CHECK(vkCreateWin32SurfaceKHR(context->instance, &surface_info, VK_NULL_HANDLE, &context->surface));
    }

    // Choose GPU
    {
        context->graphics_idx = -1;
        uint32_t gpu_count = 0;
        //TODO: Suballocation from Main Allocation
        VkPhysicalDevice gpus[10];
        VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &gpu_count, 0));
        VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &gpu_count, gpus));

        for(uint32_t i = 0; i < 1; i++)
        {
            VkPhysicalDevice gpu = gpus[i];

            uint32_t queue_family_count = 0;
            //TODO: Suballocation from Main Allocation
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
    }

    // Logical device
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

    // Swapchain
    {
        uint32_t format_count = 0;
        //TODO: Suballocation from Main Allocation
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

        // Create the image views
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
    }

    // Render Pass
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

    // Framebuffers
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

    // Descriptor Set Layouts
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorCount = 1;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo dsl_info = {};
        dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dsl_info.bindingCount = 1;
        dsl_info.pBindings = &binding;

        VK_CHECK(vkCreateDescriptorSetLayout(context->device, &dsl_info, VK_NULL_HANDLE, &context->ds_layout));
    }

    // Pipeline Layout
    {
        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &context->ds_layout;
        VK_CHECK(vkCreatePipelineLayout(context->device, &pipeline_layout_info, VK_NULL_HANDLE, &context->pipeline_layout));
    }

    // Pipeline
    {
        VkShaderModule vertex_shader, fragment_shader;

        uint32_t size_in_bytes;
        char* code = platform_read_file((char*)"assets/shaders/shader.vert.spv", &size_in_bytes);

        VkShaderModuleCreateInfo shader_info = {};
        shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_info.pCode = (uint32_t *)code;
        shader_info.codeSize = size_in_bytes;
        VK_CHECK(vkCreateShaderModule(context->device, &shader_info, VK_NULL_HANDLE, &vertex_shader));
        //TODO: Suballocation from Main Allocation
        delete code;

        code = platform_read_file((char*)"assets/shaders/shader.frag.spv", &size_in_bytes);
        shader_info.pCode = (uint32_t *)code;
        shader_info.codeSize = size_in_bytes;
        VK_CHECK(vkCreateShaderModule(context->device, &shader_info, VK_NULL_HANDLE, &fragment_shader));
        //TODO: Suballocation from Main Allocation
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
        rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

    // Command Pool
    {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = context->graphics_idx;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(context->device, &pool_info, VK_NULL_HANDLE, &context->command_pool));
    }

    // Command Buffers
    {
        auto alloc_info = cmd_alloc_info(context->command_pool);
        VK_CHECK(vkAllocateCommandBuffers(context->device, &alloc_info, &context->cmd));
    }

    // Sync Objects (Semaphores)
    {
        VkSemaphoreCreateInfo sema_info = {};
        sema_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(context->device, &sema_info, VK_NULL_HANDLE, &context->aquire_semaphore));
        VK_CHECK(vkCreateSemaphore(context->device, &sema_info, VK_NULL_HANDLE, &context->submit_semaphore));
    }

    // Sync Objects (Fence)
    {
        auto info = fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
        VK_CHECK(vkCreateFence(context->device, &info, VK_NULL_HANDLE, &context->fence));
    }

    // Staging Buffer
    {
        context->staging_buffer = vk_allocate_buffer(
            context->device, 
            context->gpu, 
            MB(1), 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    // Create Image
    {
        uint32_t file_size;
        DDS_FILE* file = (DDS_FILE*)platform_read_file((char*)"assets/textures/cakez.DDS", &file_size);
        uint32_t texture_size = file->header.Width * file->header.Height * 4u;

        vk_copy_to_buffer(&context->staging_buffer, &file->data_begin, texture_size);

        //TODO: Assertions
        context->image = vk_allocate_image(context->device, context->gpu, file->header.Width, file->header.Height, VK_FORMAT_R8G8B8A8_UNORM);

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
        img_mem_barrier.image = context->image.image;
        img_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_mem_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        img_mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        img_mem_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        img_mem_barrier.subresourceRange = range;
        
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &img_mem_barrier);
        

        VkBufferImageCopy copy_region = {};
        copy_region.imageExtent = {file->header.Width, file->header.Height, 1u};
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        vkCmdCopyBufferToImage(cmd, context->staging_buffer.buffer, context->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

        img_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        img_mem_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        img_mem_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &img_mem_barrier);

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkFence upload_fence;
        auto f_info = fence_info();
        VK_CHECK(vkCreateFence(context->device, &f_info, VK_NULL_HANDLE, &upload_fence));

        auto s_info = submit_info(&cmd);
        VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &s_info, upload_fence));

        VK_CHECK(vkWaitForFences(context->device, 1, &upload_fence, true, UINT64_MAX));
    }

    // Create Image View
    {
        VkImageViewCreateInfo img_info = {};
        img_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        img_info.image = context->image.image;
        img_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        img_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        img_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_info.subresourceRange.layerCount = 1;
        img_info.subresourceRange.levelCount = 1;


        VK_CHECK(vkCreateImageView(context->device, &img_info, VK_NULL_HANDLE, &context->image.view));
    }

    // Create Sampler
    {
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.magFilter = VK_FILTER_NEAREST;

        VK_CHECK(vkCreateSampler(context->device, &sampler_info, VK_NULL_HANDLE, &context->sampler));
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize pool_size = {};
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = 1;

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.maxSets = 1;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &pool_size;

        VK_CHECK(vkCreateDescriptorPool(context->device, &pool_info, VK_NULL_HANDLE, &context->descriptor_pool));
    }

    // Create Descriptor Set
    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.pSetLayouts = &context->ds_layout;
        alloc_info.descriptorSetCount = 1;
        alloc_info.descriptorPool = context->descriptor_pool;

        VK_CHECK(vkAllocateDescriptorSets(context->device, &alloc_info, &context->descriptor_set));
    }

    // Update Descriptor Set
    {
        VkDescriptorImageInfo img_info = {};
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_info.imageView = context->image.view;
        img_info.sampler = context->sampler;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = context->descriptor_set;
        write.pImageInfo = &img_info;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        vkUpdateDescriptorSets(context->device, 1, &write, 0, VK_NULL_HANDLE);
    }

    return true;
}

bool vk_render(VkContext* context)
{
    uint32_t img_idx;

    // We wait on the GPU to be done with the work
    VK_CHECK(vkWaitForFences(context->device, 1, &context->fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(context->device, 1, &context->fence));

    VK_CHECK(vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX, context->aquire_semaphore, VK_NULL_HANDLE, &img_idx));

    VkCommandBuffer cmd = context->cmd;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    VkClearValue clear_value = {};
    clear_value.color = {0.5f, 0.0f, 1.0f, 1.0f};

    VkRenderPassBeginInfo rp_begin_info = {};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = context->render_pass;
    rp_begin_info.renderArea.extent = context->screen_size;
    rp_begin_info.framebuffer = context->framebuffers[img_idx];
    rp_begin_info.pClearValues = &clear_value;
    rp_begin_info.clearValueCount = 1;


    vkCmdBeginRenderPass(cmd, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    // Rendering Commands
    {
        VkViewport viewport = {};
        viewport.width = context->screen_size.width;
        viewport.height =context->screen_size.height;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor = {};
        scissor.extent = context->screen_size;

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline_layout, 0, 1, &context->descriptor_set, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline);
        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

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
