#pragma once

struct vec4
{
    union
    {
        struct
        {
            float r, g, b, a;
        };
        struct
        {
            float x, y, z, w;
        };
    };
    
     bool operator==(vec4 other)
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};
