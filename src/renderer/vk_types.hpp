#pragma once

#include "logger.hpp"

#include <vulkan/vulkan.h>

#define VK_CHECK(result)                        \
    if(result != VK_SUCCESS) {                  \
        JONO_ERROR("Vulkan Error: %d", result); \
        __debugbreak();                         \
    }

struct Image
{
   VkImage image;
   VkDeviceMemory memory;
   VkImageView view;
};

struct Buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint32_t size;
    void* data;
};

struct DescriptorInfo
{
    union{
        VkDescriptorBufferInfo buffer_info;
        VkDescriptorImageInfo image_info;
    };

    DescriptorInfo(VkSampler sampler, VkImageView image_view)
    {
        image_info.sampler = sampler;
        image_info.imageView = image_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    DescriptorInfo(VkBuffer buffer)
    {
        buffer_info.buffer = buffer;
        buffer_info.offset = 0;
        buffer_info.range = VK_WHOLE_SIZE;
    }

    DescriptorInfo(VkBuffer buffer, uint32_t offset, uint32_t range)
    {
        buffer_info.buffer = buffer;
        buffer_info.offset = offset;
        buffer_info.range = range;
    }
};

