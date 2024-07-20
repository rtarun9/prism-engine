#include <windows.h>

#include "types.h"

// Global variables (which will be removed later).

// Explanation of the rendering logic:
// The engine allocates memory for its own bitmap and renders into it. GDI's (graphics device interface) role is to
// simply 'Bit' or copy the rendered bitmap into the actual device context.
typedef struct
{
    BITMAPINFO bitmap_info;
    u8 *backbuffer;
    i32 bitmap_width;
    i32 bitmap_height;
} offscreen_framebuffer_t;

typedef struct
{
    i32 width;
    i32 height;
} dimensions_t;

global_variable offscreen_framebuffer_t g_offscreen_framebuffer;

global_variable HDC g_window_device_context;

global_variable i32 g_blue_offset;
global_variable i32 g_green_offset;

internal dimensions_t get_dimensions_for_window(const HWND window_handle)
{
    RECT client_rect = {};
    GetClientRect(window_handle, &client_rect);

    dimensions_t dimensions = {};
    dimensions.width = client_rect.right - client_rect.left;
    dimensions.height = client_rect.bottom - client_rect.top;

    return dimensions;
}

internal void draw_to_backbuffer(const i32 blue_offset, const i32 green_offset)
{
    // For now, fill in the buffer with data based on pixel position.
    u8 *row = (u8 *)g_offscreen_framebuffer.backbuffer;
    const u32 row_stride = g_offscreen_framebuffer.bitmap_width * 4;

    for (i32 y = 0; y < g_offscreen_framebuffer.bitmap_height; ++y)
    {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < g_offscreen_framebuffer.bitmap_width; x++)
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
    if (g_offscreen_framebuffer.backbuffer)
    {
        VirtualFree(g_offscreen_framebuffer.backbuffer, 0, MEM_RELEASE);
    }

    g_offscreen_framebuffer.bitmap_width = width;
    g_offscreen_framebuffer.bitmap_height = height;

    const i32 backbuffer_size_in_bytes = 4 * width * height;
    g_offscreen_framebuffer.backbuffer = VirtualAlloc(NULL, backbuffer_size_in_bytes, MEM_COMMIT, PAGE_READWRITE);

    // Setup the bitmap info struct

    // note(rtarun9): the negative height, this is to ensure that the bitmap is a top down DIB.
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biWidth = width;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biHeight = -1 * height;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biPlanes = 1;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biBitCount = 32;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biCompression = BI_RGB;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biSizeImage = 0;

    draw_to_backbuffer(g_blue_offset, g_green_offset);
}

internal void update_backbuffer(const HDC device_context, const i32 window_width, const i32 window_height)
{
    StretchDIBits(device_context, 0, 0, window_width, window_height, 0, 0, g_offscreen_framebuffer.bitmap_width,
                  g_offscreen_framebuffer.bitmap_height, (void *)g_offscreen_framebuffer.backbuffer,
                  &g_offscreen_framebuffer.bitmap_info, DIB_RGB_COLORS, SRCCOPY);
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

        dimensions_t client_dimensions = get_dimensions_for_window(window_handle);
        HDC handle_to_device_context = BeginPaint(window_handle, &ps);
        update_backbuffer(g_window_device_context, client_dimensions.width, client_dimensions.height);

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
    // Since CS_OWNDC is used, the device context can be fetched once and reused multiple times.
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

    g_window_device_context = GetDC(window_handle);

    ShowWindow(window_handle, SW_SHOWNORMAL);

    resize_bitmap(1080, 720);

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

        draw_to_backbuffer(g_blue_offset, g_green_offset);

        dimensions_t client_dimensions = get_dimensions_for_window(window_handle);
        update_backbuffer(g_window_device_context, client_dimensions.width, client_dimensions.height);

        ++g_blue_offset;
        g_green_offset += 2;
    }

    ReleaseDC(window_handle, g_window_device_context);

    return 0;
}
