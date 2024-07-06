#pragma once
#include "platform.hpp"

struct DDS_PIXELFORMAT{
            uint32_t Size;
            uint32_t Flags;
            uint32_t FourCC;
            uint32_t RGBBitCount;
            uint32_t RBitMask;
            uint32_t GBitMask;
            uint32_t BBitMask;
            uint32_t ABitMask;
        };

        struct DDS_HEADER{
            uint32_t           Size;
            uint32_t           Flags;
            uint32_t           Height;
            uint32_t           Width;
            uint32_t           PitchOrLinearSize;
            uint32_t           Depth;
            uint32_t           MipMapCount;
            uint32_t           Reserved1[11];
            DDS_PIXELFORMAT    ddspf;
            uint32_t           Caps;
            uint32_t           Caps2;
            uint32_t           Caps3;
            uint32_t           Caps4;
            uint32_t           Reserved2;
        };

        struct DDS_FILE
        {
            char magic[4];
            DDS_HEADER header;
            char data_begin;
        };