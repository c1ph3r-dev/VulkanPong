#include <vulkan/vulkan.h>
#include <iostream>

int main(){
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Pong";
    app_info.pEngineName = "Ponggine";

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;

    VkInstance instance;

    auto result = vkCreateInstance(&instance_info, 0, &instance);
    if(result == VK_SUCCESS) 
        std::cout << "Successfully created vulkan  instance" << std::endl;

    return 0;
}