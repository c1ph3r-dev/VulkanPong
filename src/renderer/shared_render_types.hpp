#ifdef PONGGINE
#pragma once

#include <type_traits>
#endif

struct GlobalData
{
    int screen_size_x, screen_size_y;
};


struct Transform
{
    float x, y;
    float size_x, size_y;

#ifdef PONGGINE
    Transform() : x(0.0f), y(0.0f), size_x(1.0f), size_y(1.0f) {}

    template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>
    Transform(const T& X, const T& Y, const T& Size_x, const T& Size_y) : x((float)X), y((float)Y), size_x((float)Size_x), size_y((float)Size_y) {}

    ~Transform() = default;
#endif
};
