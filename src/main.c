#include <windows.h>

#include "types.h"

// Global variables (which will be removed later).

// Explanation of the rendering logic:
// The engine allocates memory for its own bitmap and renders into it. GDI's (graphics device interface) role is to
// simply 'Bit' or copy the rendered bitmap into the actual device context.
global_variable BITMAPINFO g_bitmap_info;
global_variable u8 *g_backbuffer;
global_variable i32 g_bitmap_width;
global_variable i32 g_bitmap_height;

internal void draw_to_backbuffer(const i32 blue_offset, const i32 green_offset)
{
    // For now, fill in the buffer with data based on pixel position.
    u8 *row = (u8 *)g_backbuffer;
    const u32 row_stride = g_bitmap_width * 4;

    for (i32 y = 0; y < g_bitmap_height; ++y)
    {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < g_bitmap_width; x++)
        {
            // Each pixel represents a RGBX value, which in memory is lied out as:
            // BB GG RR xx
            u8 blue = x + blue_offset;
            u8 green = y + green_offset;
            *pixel++ = (green << 8) | blue;
        }

        row += row_stride;
    }
}

// Function to create / resize the bitmap info header and memory for backbuffer.
internal void resize_bitmap(const i32 width, const i32 height)
{
    if (g_backbuffer)
    {
        VirtualFree(g_backbuffer, 0, MEM_RELEASE);
    }

    g_bitmap_width = width;
    g_bitmap_height = height;

    const i32 backbuffer_size_in_bytes = 4 * width * height;
    g_backbuffer = VirtualAlloc(NULL, backbuffer_size_in_bytes, MEM_COMMIT, PAGE_READWRITE);

    // Setup the bitmap info struct

    // Note the negative height, this is to ensure that the bitmap is a top down DIB.
    g_bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bitmap_info.bmiHeader.biWidth = width;
    g_bitmap_info.bmiHeader.biHeight = -1 * height;
    g_bitmap_info.bmiHeader.biPlanes = 1;
    g_bitmap_info.bmiHeader.biBitCount = 32;
    g_bitmap_info.bmiHeader.biCompression = BI_RGB;
    g_bitmap_info.bmiHeader.biSizeImage = 0;
}

internal void update_backbuffer(const HDC device_context, RECT *rect)
{

    StretchDIBits(device_context, 0, 0, g_bitmap_width, g_bitmap_height, 0, 0, rect->right - rect->left,
                  rect->bottom - rect->top, (void *)g_backbuffer, &g_bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK window_proc(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        // WM_CLOSE is called when the window is closed (shortcut key or the X button).
    case WM_CLOSE: {
        DestroyWindow(window_handle);
    }
    break;

    // Posted when DestroyWindow is called.
    case WM_DESTROY: {
        PostQuitMessage(0);
    }
    break;

    case WM_PAINT: {
        PAINTSTRUCT ps = {};

        HDC handle_to_device_context = BeginPaint(window_handle, &ps);
        RECT client_rect = {};
        GetClientRect(window_handle, &client_rect);
        update_backbuffer(handle_to_device_context, &client_rect);

        EndPaint(window_handle, &ps);
    }
    break;

    case WM_KEYDOWN: {
        if (wparam == VK_ESCAPE)
        {
            DestroyWindow(window_handle);
        }
    }
    break;

    case WM_SIZE: {
        RECT client_rect = {};
        GetClientRect(window_handle, &client_rect);

        const i32 width = client_rect.right - client_rect.left;
        const i32 height = client_rect.bottom - client_rect.top;

        resize_bitmap(width, height);
    }
    break;

    default: {
        return DefWindowProc(window_handle, message, wparam, lparam);
    }
    break;
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_command)
{
    // Create the window class, which defines a set of behaviours that multiple windows might have in common.
    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = window_proc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance;
    window_class.lpszClassName = "BaseWindowClass";

    if (!RegisterClassA(&window_class))
    {
        OutputDebugStringA("Failed to create window class.");
    }

    // Create a window.
    HWND window_handle = CreateWindowA("BaseWindowClass", "prism-engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                       CW_USEDEFAULT, 1080, 720, NULL, NULL, instance, NULL);
    if (window_handle == NULL)
    {
        OutputDebugStringA("Failed to create window.");
    }

    ShowWindow(window_handle, SW_SHOWNORMAL);

    i32 blue_offset = 0;
    i32 green_offset = 0;

    // Main game loop.
    while (1)
    {
        MSG message = {};
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        // WM_QUIT message is posted when PostQuitMessage is called.
        if (message.message == WM_QUIT)
        {
            break;
        }

        draw_to_backbuffer(blue_offset, green_offset);

        const HDC device_context = GetDC(window_handle);

        RECT client_rect = {};
        GetClientRect(window_handle, &client_rect);

        update_backbuffer(device_context, &client_rect);

        ReleaseDC(window_handle, device_context);

        ++blue_offset;
        green_offset += 2;
    }

    return 0;
}
