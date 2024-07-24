#include "../game.c"

#include <windows.h>

// Global variables (which will be removed later).

// Explanation of the rendering logic:
// The engine allocates memory for its own bitmap and renders into it. GDI's
// (graphics device interface) role is to simply 'Bit' or copy the rendered
// bitmap into the actual device context.
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
    g_offscreen_framebuffer.backbuffer = VirtualAlloc(
        NULL, backbuffer_size_in_bytes, MEM_COMMIT, PAGE_READWRITE);

    // Setup the bitmap info struct

    // note(rtarun9): the negative height, this is to ensure that the bitmap is
    // a top down DIB.
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biWidth = width;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biHeight = -1 * height;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biPlanes = 1;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biBitCount = 32;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biCompression = BI_RGB;
    g_offscreen_framebuffer.bitmap_info.bmiHeader.biSizeImage = 0;

    game_framebuffer_t game_framebuffer = {};
    game_framebuffer.backbuffer = g_offscreen_framebuffer.backbuffer;
    game_framebuffer.width = g_offscreen_framebuffer.bitmap_width;
    game_framebuffer.height = g_offscreen_framebuffer.bitmap_height;

    game_render(&game_framebuffer, g_blue_offset, g_green_offset);
}

internal void update_backbuffer(const HDC device_context,
                                const i32 window_width, const i32 window_height)
{
    StretchDIBits(device_context, 0, 0, window_width, window_height, 0, 0,
                  g_offscreen_framebuffer.bitmap_width,
                  g_offscreen_framebuffer.bitmap_height,
                  (void *)g_offscreen_framebuffer.backbuffer,
                  &g_offscreen_framebuffer.bitmap_info, DIB_RGB_COLORS,
                  SRCCOPY);
}

LRESULT CALLBACK window_proc(HWND window_handle, UINT message, WPARAM wparam,
                             LPARAM lparam)
{
    switch (message)
    {
        // WM_CLOSE is called when the window is closed (shortcut key or the X
        // button).
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

        dimensions_t client_dimensions =
            get_dimensions_for_window(window_handle);
        HDC handle_to_device_context = BeginPaint(window_handle, &ps);
        update_backbuffer(g_window_device_context, client_dimensions.width,
                          client_dimensions.height);

        EndPaint(window_handle, &ps);
    }
    break;

    case WM_KEYDOWN:
    case WM_KEYUP: {
        // Note : the 31st bit of lparam is 0 for WM_KEYDOWN and 1 for WM_KEYUP
        const b32 is_key_down = (lparam & (1 << 31)) == 0;

        if (is_key_down && wparam == VK_ESCAPE)
        {
            DestroyWindow(window_handle);
        }

        if (is_key_down)
        {
            switch (wparam)
            {
            case VK_LEFT: {
                g_blue_offset++;
            }
            break;

            case VK_RIGHT: {
                g_blue_offset--;
            }
            break;

            case VK_UP: {
                g_green_offset++;
            }
            break;

            case VK_DOWN: {
                g_green_offset--;
            }
            break;
            }
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

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance,
                   LPSTR command_line, int show_command)
{
    // Get the number of increments / counts the high performance counter does
    // in a single second. According to MSDN, the high perf counter has
    // granularity in the < 1 micro second space. The number of counter
    // increments per second is fixed, and can be fetched at application startup
    // alone.
    LARGE_INTEGER counts_per_second = {};
    QueryPerformanceFrequency(&counts_per_second);

    // Create the window class, which defines a set of behaviours that multiple
    // windows might have in common. Since CS_OWNDC is used, the device context
    // can be fetched once and reused multiple times.
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
    HWND window_handle = CreateWindowA(
        "BaseWindowClass", "prism-engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, 1080, 720, NULL, NULL, instance, NULL);
    if (window_handle == NULL)
    {
        OutputDebugStringA("Failed to create window.");
    }

    g_window_device_context = GetDC(window_handle);

    ShowWindow(window_handle, SW_SHOWNORMAL);

    resize_bitmap(1080, 720);

    // In the main loop, the counter is queried only once, at the end of the
    // frame. This is to ensure that nothing is missed between the end of loop,
    // and the start (and by doing so there is only one query performance
    // counter function call per loop iteration).
    LARGE_INTEGER end_counter = {};
    QueryPerformanceCounter(&end_counter);

    // RDTSC stands for read timestamp counter. Each processes will have a time
    // stamp counter, which basically increments after each clock cycle. The
    // __rdtsc call is a actually not a function call, but a intrinsic, which
    // tells the compiler to insert a assembly language call in place of the
    // function definition. For example, when the dis-assembly is examined, the
    // rdtsc x86 call is inserted rather than a function call. This can cause
    // some problems when instruction reordering happens, since the asm call may
    // not be reordered.
    u64 end_timestamp_counter = __rdtsc();

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

        game_framebuffer_t game_framebuffer = {};
        game_framebuffer.backbuffer = g_offscreen_framebuffer.backbuffer;
        game_framebuffer.width = g_offscreen_framebuffer.bitmap_width;
        game_framebuffer.height = g_offscreen_framebuffer.bitmap_height;

        game_render(&game_framebuffer, g_blue_offset, g_green_offset);

        dimensions_t client_dimensions =
            get_dimensions_for_window(window_handle);

        {
            // First the time (in ms) that update_backbuffer takes.
            LARGE_INTEGER blit_start_counter = {};
            QueryPerformanceCounter(&blit_start_counter);

            update_backbuffer(g_window_device_context, client_dimensions.width,
                              client_dimensions.height);

            LARGE_INTEGER blit_end_counter = {};
            QueryPerformanceCounter(&blit_end_counter);

            i64 counter_difference =
                blit_end_counter.QuadPart - blit_start_counter.QuadPart;
            i64 ms_per_frame =
                (1000 * counter_difference) / counts_per_second.QuadPart;

            char counter_difference_str[256];
            wsprintf(
                counter_difference_str,
                "Blit (update_backbuffer function) Counter Difference : %d, MS "
                "Per Frame : %d\n",
                (i32)counter_difference, (i32)ms_per_frame);
            OutputDebugStringA(counter_difference_str);
        }

        LARGE_INTEGER current_counter = {};
        QueryPerformanceCounter(&current_counter);

        i64 counter_difference =
            current_counter.QuadPart - end_counter.QuadPart;

        end_counter = current_counter;

        u64 current_timestamp_counter = __rdtsc();
        u64 timestamp_difference =
            current_timestamp_counter - end_timestamp_counter;

        end_timestamp_counter = current_timestamp_counter;

        i64 ms_per_frame =
            (1000 * counter_difference) / counts_per_second.QuadPart;
        i64 fps = counts_per_second.QuadPart / counter_difference;

        char counter_difference_str[256];
        wsprintf(counter_difference_str,
                 "Counter Difference : %d, MS Per Frame : %d, FPS : %d, RDTSC "
                 "Difference : %d\n",
                 (i32)counter_difference, (i32)ms_per_frame, (i32)fps,
                 (i32)timestamp_difference);
        OutputDebugStringA(counter_difference_str);
    }

    ReleaseDC(window_handle, g_window_device_context);

    return 0;
}
