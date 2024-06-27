#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <iostream>

#include "vk_init.hpp"

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

#define VK_CHECK(result)                        \
    if(result != VK_SUCCESS) {                  \
        std::cout << result << std::endl;       \
        __debugbreak();                         \
        return false;                           \
    }

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageFlags,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    std::cout << "Validation Error: " << pCallbackData->pMessage << std::endl;
    return false;
}

struct VkContext
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_format;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphics_queue;
    VkSwapchainKHR swapchain;
    VkCommandPool command_pool;

    VkSemaphore aquire_semaphore;
    VkSemaphore submit_semaphore;

    uint32_t swapchain_image_count;
    //TODO: Suballocation from Main Allocation
    VkImage swapchain_images[5];

    int graphics_idx;
};

bool vk_init(VkContext* context, void* window) {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Pong";
    app_info.pEngineName = "Ponggine";

    char* extensions[] = {
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
    };

    char* layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.ppEnabledExtensionNames = extensions;
    instance_info.enabledExtensionCount = ArraySize(extensions);
    instance_info.ppEnabledLayerNames = layers;
    instance_info.enabledLayerCount = ArraySize(layers);
    VK_CHECK(vkCreateInstance(&instance_info, 0, &context->instance));

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");

    if(vkCreateDebugUtilsMessengerEXT)
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_info = {};
        debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debug_info.pfnUserCallback = debug_callback;

        vkCreateDebugUtilsMessengerEXT(context->instance, &debug_info, 0, &context->debug_messenger);
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
        VK_CHECK(vkCreateWin32SurfaceKHR(context->instance, &surface_info, 0, &context->surface));
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

        char *extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkDeviceCreateInfo device_info = {};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.queueCreateInfoCount = 1;
        device_info.ppEnabledExtensionNames = extensions;
        device_info.enabledExtensionCount = ArraySize(extensions);

        VK_CHECK(vkCreateDevice(context->gpu, &device_info, 0, &context->device));

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
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->gpu, context->surface, &surface_caps)); //ERROR -1000000000 NVIDIA, -13 AMD
        uint32_t image_count = surface_caps.minImageCount + 1;
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
    }

    // Command Pool
    {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = context->graphics_idx;
        VK_CHECK(vkCreateCommandPool(context->device, &pool_info, 0, &context->command_pool));
    }

    // Sync Objects (Semaphores)
    {
        VkSemaphoreCreateInfo sema_info = {};
        sema_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(context->device, &sema_info, 0, &context->aquire_semaphore));
        VK_CHECK(vkCreateSemaphore(context->device, &sema_info, 0, &context->submit_semaphore));
    }

    return true;
}

bool vk_render(VkContext* context)
{
    uint32_t img_idx;
    vkWaitForFences(context->device, )
    VK_CHECK(vkAcquireNextImageKHR(context->device, context->swapchain, 0, context->aquire_semaphore, 0, &img_idx));

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandBufferCount = 1;
    alloc_info.commandPool = context->command_pool;
    VK_CHECK(vkAllocateCommandBuffers(context->device, &alloc_info, &cmd));

    VkCommandBufferBeginInfo begin_info = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    // Rendering Commands
    {
        VkClearColorValue color = {0.5f, 0.0f, 1.0f, 1.0f};
        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.layerCount = 1;
        range.levelCount = 1;

        vkCmdClearColorImage(cmd, context->swapchain_images[img_idx], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &color, 1, &range);
    }

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    submit_info.pSignalSemaphores = &context->submit_semaphore;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &context->aquire_semaphore;
    submit_info.waitSemaphoreCount = 1;
    VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &submit_info, 0));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pSwapchains = &context->swapchain;
    present_info.swapchainCount = 1;
    present_info.pImageIndices = &img_idx;
    present_info.pWaitSemaphores = &context->submit_semaphore;
    present_info.waitSemaphoreCount = 1;
    VK_CHECK(vkQueuePresentKHR(context->graphics_queue, &present_info));

    vkFreeCommandBuffers(context->device, context->command_pool, 1, &cmd);

    return true;
}
