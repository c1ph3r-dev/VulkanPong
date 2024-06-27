#include <windows.h>

#include "defines.hpp"
#include <renderer/vk_renderer.cpp>

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


int main(){
    VkContext context = {};

    if (!platform_create_window())
    {
        return -1;
    }

    if(!vk_init(&context, window))
    {
        return -1;
    }

    while (running)
    {
        platform_update_window(window);
        vk_render(&context);
    }
    
    return 0;
}

void platform_get_window_size(uint32_t* width, uint32_t* height)
{
    RECT rect;
    GetClientRect(window, &rect);

    *width= rect.right - rect.left;
    *height = rect.bottom - rect.top;
}
