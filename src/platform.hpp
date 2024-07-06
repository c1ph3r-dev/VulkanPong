#pragma once

#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "defines.hpp"

enum TextColor {
    TEXT_COLOR_WHITE,
    TEXT_COLOR_GREEN,
    TEXT_COLOR_YELLOW,
    TEXT_COLOR_RED,
    TEXT_COLOR_INTENSE_RED
};

void platform_get_window_size(uint32_t *width, uint32_t *height);

char* platform_read_file(char* path, uint32_t* length);

void platform_log(const char* message, TextColor color);

#endif // PLATFORM_HPP