#version 450

#include "../../src/renderer/shared_render_types.hpp"

layout(location = 0) out vec2 uv;

layout(set = 0, binding = 0) uniform GlobalUBO
{
    GlobalData global_data;
};

layout(set = 0, binding = 1) readonly buffer Transforms
{
    Transform transforms[];
};

Transform transform = transforms[gl_InstanceIndex];

vec4 vertices[4] = 
{
    //top left
    vec4(transform.x, transform.y, 0.0, 0.0),

    //bottom left
    vec4(transform.x, transform.y + transform.size_y, 0.0, 1.0),

    //bottom right
    vec4(transform.x + transform.size_x, transform.y + transform.size_y, 1.0, 1.0),

    //top right
    vec4(transform.x + transform.size_x, transform.y, 1.0, 0.0)

};

void main()
{
    vec2 normalized_pos = 2.0 * vec2(vertices[gl_VertexIndex].x / global_data.screen_size_x, vertices[gl_VertexIndex].y / global_data.screen_size_y) - 1.0;
    gl_Position = vec4(normalized_pos, 1.0, 1.0);
    uv = vertices[gl_VertexIndex].zw;
}