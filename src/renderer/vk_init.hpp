#pragma once
#include <vulkan/vulkan.h>

#ifndef VK_INIT_HPP_
#define VK_INIT_HPP_
VkCommandBufferBeginInfo cmd_begin_info()
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    return info;
}
#endif //VK_INIT_HPP_