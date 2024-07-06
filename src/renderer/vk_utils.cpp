#include "vk_types.hpp"

// TODO: Just so vscode doesn't complain about memcpy
#include <cstring>

uint32_t vk_get_memory_type_index(
    VkPhysicalDevice gpu,
    VkMemoryRequirements mem_reqs,
    VkMemoryPropertyFlags memory_properties)
{
    uint32_t typeIdx = INVALID_IDX;

    VkPhysicalDeviceMemoryProperties gpu_mem_props;
    vkGetPhysicalDeviceMemoryProperties(gpu, &gpu_mem_props);

    for(uint32_t i = 0; i < gpu_mem_props.memoryTypeCount; i++)
    {
        if(mem_reqs.memoryTypeBits & (1 << i) &&
        (gpu_mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            typeIdx = i;
        }
    }

    JONO_ASSERT(typeIdx != INVALID_IDX, "Failed to find property type Index for Memory Properties: %d", memory_properties);

    return typeIdx;
}

Image vk_allocate_image(
    VkDevice device,
    VkPhysicalDevice gpu,
    uint32_t width, 
    uint32_t height,
    VkFormat format)
{
    Image image = {};

    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.extent = {width, height, 1u};
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(vkCreateImage(device, &image_info, VK_NULL_HANDLE, &image.image));

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, image.image, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = vk_get_memory_type_index(gpu, mem_reqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device, &alloc_info, VK_NULL_HANDLE, &image.memory));
    VK_CHECK(vkBindImageMemory(device, image.image, image.memory, 0));

    return image;
}

Buffer vk_allocate_buffer(
    VkDevice device,
    VkPhysicalDevice gpu,
    uint32_t size,
    VkBufferUsageFlags buffer_usage,
    VkMemoryPropertyFlags memory_properties)
{
    Buffer buffer = {};
    buffer.size = size;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.usage = buffer_usage;
    buffer_info.size = size;

    VK_CHECK(vkCreateBuffer(device, &buffer_info, VK_NULL_HANDLE, &buffer.buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, buffer.buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = buffer.size;
    alloc_info.memoryTypeIndex = vk_get_memory_type_index(gpu, mem_reqs, memory_properties);

    VK_CHECK(vkAllocateMemory(device, &alloc_info, VK_NULL_HANDLE, &buffer.memory));
    
    // Only map memory we can actually write from the cpu
    if(memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        VK_CHECK(vkMapMemory(device, buffer.memory, 0, MB(1), 0, &buffer.data));
    }
    
    VK_CHECK(vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0));

    return buffer;
}

void vk_copy_to_buffer(
    Buffer* buffer,
    void* data,
    uint32_t size)
{
    JONO_ASSERT(buffer->size >= size, "Buffer too small: %d for data: %d", buffer->size, size);

    // If we have mapped data
    if(buffer->data)
    {
        memcpy(buffer->data, data, size);
    }
    else
    {
        // TODO: Implement, copy data using command buffer
    }
}
