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

VkCommandBufferAllocateInfo cmd_alloc_info(VkCommandPool command_pool)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandBufferCount = 1;
    info.commandPool = command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    return info;
}

VkFenceCreateInfo fence_info(VkFenceCreateFlags flags = 0)
{
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = flags;

    return info;
}

VkSubmitInfo submit_info(VkCommandBuffer* cmd, uint32_t cmd_count = 1)
{
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = cmd_count;
    info.pCommandBuffers = cmd;

    return info;
}

VkDescriptorSetLayoutBinding layout_binding(
    VkDescriptorType type,
    VkShaderStageFlags shader_stages,
    uint32_t count,
    uint32_t binding_number)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = binding_number;
    binding.descriptorCount = count;
    binding.descriptorType = type;
    binding.stageFlags = shader_stages;

    return binding;
}

VkWriteDescriptorSet write_set(
   VkDescriptorSet set,
   VkDescriptorType type,
   DescriptorInfo* descriptor_info,
   uint32_t binding_number,
   uint32_t count)
{
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.pImageInfo = &descriptor_info->image_info;
    write.pBufferInfo = &descriptor_info->buffer_info;
    write.dstBinding = binding_number;
    write.descriptorCount = count;
    write.descriptorType = type;

    return write;
}

#endif //VK_INIT_HPP_