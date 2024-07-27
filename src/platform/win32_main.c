#include "../game.c"

#include "win32_main.h"

internal game_key_state_t
get_game_key_state(win32_keyboard_state_t *restrict current_keyboard_state,
                   win32_keyboard_state_t *restrict previous_keyboard_state,
                   unsigned char virtual_keycode)

{
    b8 is_key_down = current_keyboard_state->key_states[virtual_keycode] & 0x80;

    b8 was_key_down =
        previous_keyboard_state->key_states[virtual_keycode] & 0x80;

    game_key_state_t key_state = {0};
    key_state.is_key_down = is_key_down;
    key_state.state_changed = is_key_down != was_key_down;

    return key_state;
}

// Explanation of the rendering logic:
// The engine allocates memory for its own bitmap and renders into it. GDI's
// (graphics device interface) role is to simply 'Blit' or copy the rendered
// bitmap into the actual device context.
// The bitmap will be of fixed size and will not depend on window resolution.

global_variable win32_offscreen_framebuffer_t g_offscreen_framebuffer;

internal win32_dimensions_t get_dimensions_for_window(const HWND window_handle)
{
    RECT client_rect = {0};
    GetClientRect(window_handle, &client_rect);

    win32_dimensions_t dimensions = {0};
    dimensions.width = client_rect.right - client_rect.left;
    dimensions.height = client_rect.bottom - client_rect.top;

    return dimensions;
}

// Function to create / resize the bitmap info header and memory for backbuffer.
internal void win32_resize_bitmap(const i32 width, const i32 height)
{
    if (g_offscreen_framebuffer.backbuffer_memory)
    {
        VirtualFree(g_offscreen_framebuffer.backbuffer_memory, 0, MEM_RELEASE);
    }

    const i32 backbuffer_size_in_bytes = 4 * width * height;

    g_offscreen_framebuffer.backbuffer_memory =
        VirtualAlloc(NULL, backbuffer_size_in_bytes, MEM_COMMIT | MEM_RESERVE,
                     PAGE_READWRITE);

    // Setup the bitmap info struct

    // note(rtarun9) : the negative height, this is to ensure that the bitmap is
    // a top down DIB.
    g_offscreen_framebuffer.bitmap_info_header.biSize =
        sizeof(BITMAPINFOHEADER);
    g_offscreen_framebuffer.bitmap_info_header.biWidth = width;
    g_offscreen_framebuffer.bitmap_info_header.biHeight = -1 * height;
    g_offscreen_framebuffer.bitmap_info_header.biPlanes = 1;
    g_offscreen_framebuffer.bitmap_info_header.biBitCount = 32;
    g_offscreen_framebuffer.bitmap_info_header.biCompression = BI_RGB;
    g_offscreen_framebuffer.bitmap_info_header.biSizeImage = 0;

    game_framebuffer_t game_framebuffer = {0};
    game_framebuffer.backbuffer_memory =
        g_offscreen_framebuffer.backbuffer_memory;
    game_framebuffer.width = width;
    game_framebuffer.height = height;
}

internal void win32_update_backbuffer(const HDC device_context,
                                      const i32 window_width,
                                      const i32 window_height)
{
    BITMAPINFO bitmap_info;
    bitmap_info.bmiHeader = g_offscreen_framebuffer.bitmap_info_header;

    StretchDIBits(device_context, 0, 0, window_width, window_height, 0, 0,
                  g_offscreen_framebuffer.bitmap_info_header.biWidth,
                  g_offscreen_framebuffer.bitmap_info_header.biHeight * -1,
                  (void *)g_offscreen_framebuffer.backbuffer_memory,
                  &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK win32_window_proc(HWND window_handle, UINT message,
                                   WPARAM wparam, LPARAM lparam)
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

        win32_dimensions_t client_dimensions =
            get_dimensions_for_window(window_handle);
        HDC handle_to_device_context = BeginPaint(window_handle, &ps);
        win32_update_backbuffer(handle_to_device_context,
                                client_dimensions.width,
                                client_dimensions.height);

        EndPaint(window_handle, &ps);
    }
    break;

    case WM_KEYDOWN:
    case WM_KEYUP: {
        // note(rtarun9) : the 31st bit of lparam is 0 for WM_KEYDOWN and 1 for
        // WM_KEYUP
        const b8 is_key_down = (lparam & (1 << 31)) == 0;

        if (is_key_down && wparam == VK_ESCAPE)
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

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance,
                   LPSTR command_line, int show_command)
{
    // Get the number of increments / counts the high performance counter does
    // in a single second. According to MSDN, the high perf counter has
    // granularity in the < 1 micro second space. The number of counter
    // increments per second is fixed, and can be fetched at application startup
    // alone.
    LARGE_INTEGER counts_per_second = {0};
    QueryPerformanceFrequency(&counts_per_second);

    // Create the window class, which defines a set of behaviours that multiple
    // windows might have in common. Since CS_OWNDC is used, the device context
    // can be fetched once and reused multiple times.
    WNDCLASSA window_class = {0};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = win32_window_proc;
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

    HDC window_device_context = GetDC(window_handle);

    ShowWindow(window_handle, SW_SHOWNORMAL);

    win32_resize_bitmap(1080, 720);

    // In the main loop, the counter is queried only once, at the end of the
    // frame. This is to ensure that nothing is missed between the end of loop,
    // and the start (and by doing so there is only one query performance
    // counter function call per loop iteration).
    LARGE_INTEGER end_counter = {0};
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

    // Some explanation for the input module :
    // To determine if a key state has changed, the platform layer will store 2
    // copies of input data structures, which are swapped each frame . Finding
    // state change becomes trivial in this case.
    win32_keyboard_state_t current_keyboard_state = {0};
    win32_keyboard_state_t previous_keyboard_state = {0};

    win32_keyboard_state_t *current_keyboard_state_ptr =
        &current_keyboard_state;
    win32_keyboard_state_t *previous_keyboard_state_ptr =
        &previous_keyboard_state;

    // Allocate memory upfront.
    win32_memory_allocator_t memory_allocator = {0};
    memory_allocator.permanent_memory_size = MEGABYTE(128u);
    memory_allocator.permanent_memory =
        VirtualAlloc(NULL, memory_allocator.permanent_memory_size,
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

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

        // Fill the keyboard state struct.
        GetKeyboardState(&current_keyboard_state.key_states[0]);

        game_input_t game_input = {0};
        game_input.keyboard_state.key_w = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'W');

        game_input.keyboard_state.key_s = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'S');

        game_input.keyboard_state.key_a = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'A');

        game_input.keyboard_state.key_d = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'D');

        // note(rtarun9) : Should rendering only be done when WM_PAINT is
        // called, or should it be called in the game loop always?
        game_framebuffer_t game_framebuffer = {0};
        game_framebuffer.backbuffer_memory =
            g_offscreen_framebuffer.backbuffer_memory;
        game_framebuffer.width =
            g_offscreen_framebuffer.bitmap_info_header.biWidth;
        game_framebuffer.height =
            g_offscreen_framebuffer.bitmap_info_header.biHeight * -1;

        game_memory_allocator_t game_memory_allocator = {0};
        game_memory_allocator.permanent_memory_size =
            memory_allocator.permanent_memory_size;
        game_memory_allocator.permanent_memory =
            memory_allocator.permanent_memory;

        game_render(&game_memory_allocator, &game_framebuffer, &game_input);

        win32_dimensions_t client_dimensions =
            get_dimensions_for_window(window_handle);

        {
            // Find out how long updating the backbuffer takes.
            LARGE_INTEGER blit_start_counter = {};
            QueryPerformanceCounter(&blit_start_counter);

            win32_update_backbuffer(window_device_context,
                                    client_dimensions.width,
                                    client_dimensions.height);

            LARGE_INTEGER blit_end_counter = {};
            QueryPerformanceCounter(&blit_end_counter);

            i64 counter_difference =
                blit_end_counter.QuadPart - blit_start_counter.QuadPart;
            i64 ms_per_frame =
                (1000 * counter_difference) / counts_per_second.QuadPart;

            /*
            char counter_difference_str[256];
            wsprintf(
                counter_difference_str,
                "Blit (update_backbuffer function) Counter Difference : %d, MS "
                "Per Frame : %d\n",
                (i32)counter_difference, (i32)ms_per_frame);
            OutputDebugStringA(counter_difference_str);
            */
        }

        LARGE_INTEGER current_counter = {0};
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

        /*
        char counter_difference_str[256];
        wsprintf(counter_difference_str,
                 "Counter Difference : %d, MS Per Frame : %d, FPS : %d, RDTSC "
                 "Difference : %d\n",
                 (i32)counter_difference, (i32)ms_per_frame, (i32)fps,
                 (i32)timestamp_difference);
        OutputDebugStringA(counter_difference_str);
        */

        // Swap keyboard states.
        {
            win32_keyboard_state_t *temp = current_keyboard_state_ptr;
            current_keyboard_state_ptr = previous_keyboard_state_ptr;
            previous_keyboard_state_ptr = temp;
        }
    }

    ReleaseDC(window_handle, window_device_context);

    return 0;
}
