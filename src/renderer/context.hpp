#pragma once

#include <vulkan/vulkan.h>
#include "vk_types.hpp"

uint32_t constexpr MAX_IMAGES = 100;
uint32_t constexpr MAX_DESCRIPTORS = 100;
uint32_t constexpr MAX_RENDER_COMMANDS = 100;
uint32_t constexpr MAX_TRANSFORMS = MAX_ENTITIES;

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
    VkSampler sampler;
    VkDescriptorPool descriptor_pool;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer cmd;
    VkRenderPass render_pass;

    uint32_t transform_count;
    Transform transforms[MAX_TRANSFORMS];

    Buffer staging_buffer;
    Buffer transform_storage_buffer;
    Buffer material_storage_buffer;
    Buffer global_UBO;
    Buffer index_buffer;

    VkSemaphore aquire_semaphore;
    VkSemaphore submit_semaphore;

    VkFence fence;

    uint32_t swapchain_image_count;

    VkImage swapchain_images[5];
    VkImageView swapchain_image_views[5];
    VkFramebuffer framebuffers[5];

    uint32_t image_count;
    Image images[MAX_IMAGES];

    uint32_t descriptor_count;
    Descriptor descriptors[MAX_DESCRIPTORS];

    uint32_t render_command_count;
    RenderCommand render_commands[MAX_RENDER_COMMANDS];

    int graphics_idx;
};
