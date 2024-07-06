#pragma once

#include "platform.hpp"

// Standard Library
#include <stdio.h>

template <typename... Args>
void _log(const char *prefix, TextColor color, const char *message, Args... args)
{
    char fmtBuffer[32000] = {};
    char msgBuffer[32000] = {};

    sprintf(fmtBuffer, "%s: %s\n", prefix, message);
    sprintf(msgBuffer, fmtBuffer, args...);

    platform_log(msgBuffer, color);
}

#define JONO_TRACE(message, ...) _log("TRACE", TEXT_COLOR_GREEN, message, __VA_ARGS__)
#define JONO_WARN(message, ...) _log("WARNING", TEXT_COLOR_YELLOW, message, __VA_ARGS__)
#define JONO_ERROR(message, ...) _log("ERROR", TEXT_COLOR_RED, message, __VA_ARGS__)
#define JONO_FATAL(message, ...) _log("FATAL", TEXT_COLOR_INTENSE_RED, message, __VA_ARGS__)

#ifdef DEBUG
#define JONO_ASSERT(x, message, ...)          \
    {                                         \
        if (!(x))                             \
        {                                     \
            JONO_ERROR(message, __VA_ARGS__); \
            __debugbreak();                   \
        }                                     \
    }
#elif
#define JONO_ASSERT(x, message, ...)
#endif