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
