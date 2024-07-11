#pragma once

#include "context.hpp"

#include <assets/assets.hpp>
#include <game/game.hpp>

class VkRenderer
{
private:
    VkContext* context;

    bool create_debugger();
    void create_instance();
    void create_window(void* window);
    bool select_gpu();
    void create_device();
    void create_image_views();
    void create_swapchain();
    void create_render_pass();
    void create_framebuffers();
    void create_descriptor_set_layouts();
    void create_pipeline_layout();
    void create_pipeline();
    void create_command_pool();
    void create_sync_objects();
    void create_buffers();
    void create_sampler();
    void create_descriptor_pool();
    void create_descriptor_set();
    void update_descriptor_set();

public:
    VkRenderer() : context(new VkContext{}) {}
    ~VkRenderer() { delete context; };

    bool init(void* window);

    bool render(GameState* state);

private:
    void int_render(const VkCommandBuffer& cmd, GameState* state);

    Image* vk_create_image(AssetTypeID id);
    Image* vk_get_image(AssetTypeID id);
};
