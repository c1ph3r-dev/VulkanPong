#include "vk_shader_util.hpp"

#include <cstdlib>

void vk_compile_shader(const char* shader_path, const char* spirv_path)
{
    JONO_ASSERT(shader_path, "No shader path specified");
    JONO_ASSERT(spirv_path, "No SPIR-V path specified");

    char command[320] = {};

    sprintf(command, "lib\\glslc.exe % s -o %s", shader_path, spirv_path);
    int result = system(command);

    JONO_ASSERT(!result, "Failed to compile shader: %s", shader_path);
}