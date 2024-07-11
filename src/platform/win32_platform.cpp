#include <windows.h>

#include "defines.hpp"

#include "logger.hpp"


#include <renderer/vk_renderer.hpp>

#include "platform.hpp"

global_variable bool running = true;
global_variable HWND window;

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
    case WM_CLOSE:
        running = false;
        break;
    }

    return DefWindowProcA(window, msg, wParam, lParam);
}

bool platform_create_window()
{
    HINSTANCE instance = GetModuleHandleA(0);

    WNDCLASS wc = {};
    wc.lpfnWndProc = platform_window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = "vulkan_engine_class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if(!RegisterClass(&wc)){
        MessageBoxA(0, "Failed registering window class", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    window = CreateWindowExA(
            WS_EX_APPWINDOW,
            "vulkan_engine_class",
            "Pong",
            WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED,
            100, 100, 1600, 720, 0, 0, instance, 0);

    if(window == 0)
    {
        MessageBoxA(0, "Failed creating window", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    ShowWindow(window, SW_SHOW);
    return true;
}

void platform_update_window(HWND window)
{
    MSG msg;

    while (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

int main()
{
    VkRenderer renderer = {};
    Game game = {};

    if (!platform_create_window())
    {
        JONO_ERROR("Failed to create a window");
        return EXIT_FAILURE;
    }

    if(!renderer.init(window))
    {
        JONO_ERROR("Failed to initialize Vulkan");
        return EXIT_FAILURE;
    }

    if(!game.init())
    {
        JONO_ERROR("Failed to initialize game");
        return EXIT_FAILURE;
    }

    while (running)
    {
        platform_update_window(window);
        game.update();
        if(!renderer.render(game.GetGameState()))
        {
            JONO_ERROR("Failed to render");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}



void platform_get_window_size(uint32_t* width, uint32_t* height)
{
    RECT rect;
    GetClientRect(window, &rect);

    *width= rect.right - rect.left;
    *height = rect.bottom - rect.top;
}

char* platform_read_file(char* path, uint32_t* length)
{
    char* result = NULL;

    auto file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if(GetFileSizeEx(file, &size))
        {
            *length = (uint32_t)size.QuadPart;
            result = new char[*length];

            DWORD bytes_read = 0;
            if(ReadFile(file, result, *length, &bytes_read, 0))
            {
                // Success
            }
            else
            {
                JONO_ASSERT(0, "Failed to read file: %s", path);
                JONO_ERROR("Failed to read file: %s", path);
            }
        }
        else
        {
            JONO_ASSERT(0, "Could not get size of file: %s", path);
            JONO_ERROR("Could not get size of file: %s", path);
        }

        CloseHandle(file);
    }
    else
    {
        JONO_ASSERT(0, "Could not open file: %s", path);
        JONO_ERROR("Could not open file: %s", path);
    }


    return result;
}

void platform_log(const char* message, TextColor color)
{
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    uint32_t color_bits = 0;

    switch (color)
    {
    default:
    case TEXT_COLOR_WHITE:
        color_bits = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        break;
    case TEXT_COLOR_GREEN:
        color_bits = FOREGROUND_GREEN;
        break;
    case TEXT_COLOR_YELLOW:
        color_bits = FOREGROUND_RED | FOREGROUND_GREEN;
        break;
    case TEXT_COLOR_RED:
        color_bits = FOREGROUND_RED;
        break;
    case TEXT_COLOR_INTENSE_RED:
        color_bits = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    }

    SetConsoleTextAttribute(console_handle, color_bits);

    #ifdef DEBUG
    OutputDebugStringA(message);
    #endif

    WriteConsoleA(console_handle, message, strlen(message), 0, 0);
}
