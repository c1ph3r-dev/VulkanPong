#pragma once

#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "defines.hpp"

void platform_get_window_size(uint32_t *width, uint32_t *height);
char* platform_read_file(char* path, uint32_t* length);

#endif // PLATFORM_HPP