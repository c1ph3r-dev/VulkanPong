#include <vulkan/vulkan.h>

struct VkContext
{
    VkInstance instance;
};

bool vk_init(VkContext* context) {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Pong";
    app_info.pEngineName = "Ponggine";

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;

    auto result = vkCreateInstance(&instance_info, 0, &context->instance);
    if(result == VK_SUCCESS)
    {
        return true;
    }

    return false;
}

