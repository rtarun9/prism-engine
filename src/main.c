#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>

#include <stdint.h>

// Typedefs to primitive datatypes.
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef i32 b32;

typedef float f32;
typedef double f64;

// #defines to make usage of 'static' based on purpose more clear.
#define internal static
#define global_variable static
#define local_persist static

global_variable u32 *g_framebuffer_memory = NULL;
global_variable BITMAPINFO g_bitmap_info = {0};

internal void win32_render_framebuffer_to_window(HDC device_context, u32 width,
                                                 u32 height)
{
    StretchDIBits(device_context, 0, 0, width, height, 0, 0, width, height,
                  g_framebuffer_memory, &g_bitmap_info, DIB_RGB_COLORS,
                  SRCCOPY);
}

internal void win32_render_gradient_to_framebuffer(u32 width, u32 height)
{
    u32 *row = g_framebuffer_memory;
    u32 pitch = width;

    for (u32 y = 0; y < height; y++)
    {
        u32 *pixel = row;
        for (u32 x = 0; x < width; x++)
        {
            u8 red = (x & 0xff);
            u8 blue = (y & 0xff);
            u8 green = 0;
            u8 alpha = 0xff;

            // Layout in memory is : BB GG RR XX
            *pixel++ = blue | (green << 8) | (red << 16) | (alpha << 24);
        }

        row += pitch;
    }
}

internal void win32_resize_framebuffer(u32 width, u32 height)
{
    if (g_framebuffer_memory)
    {
        VirtualFree(g_framebuffer_memory, 0, MEM_RELEASE);
    }

    g_framebuffer_memory =
        VirtualAlloc(0, sizeof(u32) * width * height, MEM_COMMIT | MEM_RESERVE,
                     PAGE_READWRITE);

    // Update the bitmap info struct to use the new framebuffer dimensions.
    // NOTE: biHeight is negative, because it is desirable to have origin at top
    // left corner.
    g_bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bitmap_info.bmiHeader.biWidth = width;
    g_bitmap_info.bmiHeader.biHeight = -1 * height;
    g_bitmap_info.bmiHeader.biPlanes = 1;
    g_bitmap_info.bmiHeader.biBitCount = 32;
    g_bitmap_info.bmiHeader.biCompression = BI_RGB;
    g_bitmap_info.bmiHeader.biSizeImage = 0;
    g_bitmap_info.bmiHeader.biXPelsPerMeter = 0;
    g_bitmap_info.bmiHeader.biYPelsPerMeter = 0;
    g_bitmap_info.bmiHeader.biClrUsed = 0;
    g_bitmap_info.bmiHeader.biClrImportant = 0;
}

internal LRESULT CALLBACK win32_window_proc(HWND window, UINT message,
                                            WPARAM w_param, LPARAM l_param)
{
    LRESULT result = {0};

    switch (message)
    {
    case WM_SIZE: {
        RECT client_rect = {0};
        GetClientRect(window, &client_rect);

        u32 client_region_width = client_rect.right - client_rect.left;
        u32 client_region_height = client_rect.bottom - client_rect.top;

        win32_resize_framebuffer(client_region_width, client_region_height);

        OutputDebugStringW(L"WM_SIZE\n");
    }
    break;

    case WM_PAINT: {
        PAINTSTRUCT paint = {0};
        HDC device_context = BeginPaint(window, &paint);

        RECT client_rect = {0};
        GetClientRect(window, &client_rect);

        u32 client_region_width = client_rect.right - client_rect.left;
        u32 client_region_height = client_rect.bottom - client_rect.top;

        win32_render_gradient_to_framebuffer(client_region_width,
                                             client_region_height);
        win32_render_framebuffer_to_window(device_context, client_region_width,
                                           client_region_height);

        EndPaint(window, &paint);

        OutputDebugStringW(L"WM_PAINT\n");
    }
    break;

    case WM_CLOSE: {
        OutputDebugStringW(L"WM_CLOSE\n");
        DestroyWindow(window);
    }
    break;

    case WM_DESTROY: {
        OutputDebugStringW(L"WM_DESTROY\n");
        PostQuitMessage(0);
    }
    break;

    default: {
        result = DefWindowProc(window, message, w_param, l_param);
    }
    break;
    }

    return result;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR command_line, int command_show)
{
    // Register a window class : set of behaviors that several windows might
    // have in common (required even if only a single window is created by the
    // application).
    WNDCLASSEXW window_class = {0};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.style = CS_VREDRAW | CS_HREDRAW;
    window_class.lpfnWndProc = win32_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"CustomWindowClass";

    RegisterClassExW(&window_class);

    // Create the actual window.

    HWND window = CreateWindowExW(
        0, window_class.lpszClassName, L"prism-engine", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1080, 720, NULL, NULL, instance, NULL);
    ShowWindow(window, SW_SHOWNORMAL);

    if (!window)
    {
        // TODO: Logging.
        return -1;
    }

    RECT client_rect = {0};
    GetClientRect(window, &client_rect);

    u32 client_region_width = client_rect.right - client_rect.left;
    u32 client_region_height = client_rect.bottom - client_rect.top;

    win32_resize_framebuffer(client_region_width, client_region_height);

    BYTE keyboard_state[256] = {0};

    BOOL quit = FALSE;
    while (!quit)
    {
        MSG message = {0};
        while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (message.message == WM_QUIT)
        {
            quit = TRUE;
        }

        if (GetKeyboardState(&keyboard_state[0]))
        {
            if (keyboard_state[VK_ESCAPE] & 0x80)
            {
                quit = TRUE;
            }
        }

        HDC device_context = GetDC(window);
        RECT client_rect = {0};
        GetClientRect(window, &client_rect);

        u32 client_region_width = client_rect.right - client_rect.left;
        u32 client_region_height = client_rect.bottom - client_rect.top;

        win32_render_gradient_to_framebuffer(client_region_width,
                                             client_region_height);
        win32_render_framebuffer_to_window(device_context, client_region_width,
                                           client_region_height);
        ReleaseDC(window, device_context);
    }

    return 0;
}
