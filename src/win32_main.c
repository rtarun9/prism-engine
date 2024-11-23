#ifndef UNICODE
#define UNICODE
#endif

#include "common.h"

#include "game.c"

#include <Windows.h>

// NOTE: even though bitmap info has the width and the height, because a top
// down framebuffer is used, the height is inverted. Instead of inverting height
// each time it is required, 2 separate fields for width and height are created.
typedef struct
{
    u32 *framebuffer_memory;
    BITMAPINFO bitmap_info;

    u32 width;
    u32 height;
} win32_offscreen_buffer_t;

global_variable win32_offscreen_buffer_t g_backbuffer = {0};

typedef struct
{
    u32 width;
    u32 height;
} win32_window_dimensions_t;

internal win32_window_dimensions_t
win32_get_window_client_dimensions(const HWND window)
{
    win32_window_dimensions_t result = {0};

    RECT client_rect = {0};
    GetClientRect(window, &client_rect);

    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

internal void win32_render_buffer_to_window(
    const win32_offscreen_buffer_t *buffer, const HDC device_context,
    const u32 window_width, const u32 window_height)
{
    ASSERT(buffer);

    StretchDIBits(device_context, 0, 0, window_width, window_height, 0, 0,
                  buffer->width, buffer->height, buffer->framebuffer_memory,
                  &buffer->bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

internal void win32_resize_framebuffer(win32_offscreen_buffer_t *const buffer,
                                       const u32 width, const u32 height)
{
    ASSERT(buffer);

    if (buffer->framebuffer_memory)
    {
        VirtualFree(buffer->framebuffer_memory, 0, MEM_RELEASE);
    }

    buffer->framebuffer_memory =
        VirtualAlloc(0, sizeof(u32) * width * height, MEM_COMMIT | MEM_RESERVE,
                     PAGE_READWRITE);

    ASSERT(buffer->framebuffer_memory);

    // Update the bitmap info struct to use the new framebuffer dimensions.
    // NOTE: biHeight is negative, because it is desirable to have origin at top
    // left corner.
    buffer->bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    buffer->bitmap_info.bmiHeader.biWidth = width;
    buffer->bitmap_info.bmiHeader.biHeight = -1 * height;
    buffer->bitmap_info.bmiHeader.biPlanes = 1;
    buffer->bitmap_info.bmiHeader.biBitCount = 32;
    buffer->bitmap_info.bmiHeader.biCompression = BI_RGB;
    buffer->bitmap_info.bmiHeader.biSizeImage = 0;
    buffer->bitmap_info.bmiHeader.biXPelsPerMeter = 0;
    buffer->bitmap_info.bmiHeader.biYPelsPerMeter = 0;
    buffer->bitmap_info.bmiHeader.biClrUsed = 0;
    buffer->bitmap_info.bmiHeader.biClrImportant = 0;

    buffer->width = width;
    buffer->height = height;
}

internal void win32_handle_key_input(game_key_state_t *const restrict input,
                                     b32 is_key_down)
{
    if (input->is_key_down != is_key_down)
    {
        input->state_transition_count++;
    }

    input->is_key_down = is_key_down;
}

internal LRESULT CALLBACK win32_window_proc(HWND window, UINT message,
                                            WPARAM w_param, LPARAM l_param)
{
    LRESULT result = {0};

    switch (message)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        ASSERT(
            0 &&
            "Key up and key down should not be handled by the window proc!!!");
    }
    break;

    case WM_PAINT: {
        PAINTSTRUCT paint = {0};
        const HDC device_context = BeginPaint(window, &paint);

        const win32_window_dimensions_t window_dimensions =
            win32_get_window_client_dimensions(window);

        // NOTE: In WM_PAINT, the backbuffer is NOT being rendered to, but the
        // window is being rendered to directly.
        win32_render_buffer_to_window(&g_backbuffer, device_context,
                                      window_dimensions.width,
                                      window_dimensions.height);

        EndPaint(window, &paint);

        OutputDebugStringW(L"WM_PAINT\n");
    }
    break;

    case WM_CLOSE: {
        DestroyWindow(window);
        OutputDebugStringW(L"WM_CLOSE\n");
    }
    break;

    case WM_DESTROY: {
        PostQuitMessage(0);
        OutputDebugStringW(L"WM_DESTROY\n");
    }
    break;

    default: {
        result = DefWindowProc(window, message, w_param, l_param);
    }
    break;
    }

    return result;
}

internal u8 *platform_read_file(const char *file_name)
{
    ASSERT(file_name);

    HANDLE file_handle =
        CreateFileA(file_name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);

    u8 *file_buffer = NULL;
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        // Get the file size.
        LARGE_INTEGER file_size = {0};
        if (GetFileSizeEx(file_handle, &file_size))
        {
            // Allocate memory for the buffer that will contain the file
            // contents.
            file_buffer =
                (u8 *)VirtualAlloc(0, file_size.QuadPart,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            ASSERT(file_buffer);

            // Read from file finally.
            DWORD number_of_bytes_read = 0;
            if (ReadFile(file_handle, (void *)file_buffer,
                         truncate_u64_to_u32(file_size.QuadPart),
                         &number_of_bytes_read, NULL))
            {
                ASSERT(number_of_bytes_read == file_size.QuadPart);
            }
            else
            {
                VirtualFree(file_buffer, 0, MEM_RELEASE);
            }
        }
        CloseHandle(file_handle);
    }

    return file_buffer;
}

internal void platform_close_file(u8 *file_buffer)
{
    VirtualFree(file_buffer, 0, MEM_RELEASE);
}

internal b32 platform_write_to_file(const char *string, const char *file_name)
{
    ASSERT(file_name);
    ASSERT(string);

    b32 result = true;

    HANDLE file_handle =
        CreateFileA(file_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);

    if (file_handle != INVALID_HANDLE_VALUE)
    {
        // TODO: Best way to get how many bytes we have to write.
        u32 string_len = truncate_u64_to_u32(strlen(string));

        DWORD number_of_bytes_written = 0;

        if (WriteFile(file_handle, (void *)string, string_len,
                      &number_of_bytes_written, NULL))
        {
            ASSERT(number_of_bytes_written == string_len);
        }
        CloseHandle(file_handle);
    }
    else
    {
        result = false;
    }

    return result;
}

internal u64 win32_get_perf_counter_frequency()
{
    LARGE_INTEGER frequency = {0};
    QueryPerformanceFrequency(&frequency);

    return (u64)frequency.QuadPart;
}

internal u64 win32_get_perf_counter_value()
{
    LARGE_INTEGER counter_value = {0};
    QueryPerformanceCounter(&counter_value);

    return (u64)counter_value.QuadPart;
}

internal f32 win32_get_time_delta_ms(u64 start, u64 end, u64 counts_per_second)
{
    f32 result = 1000.0f * (f32)(end - start) / (f32)counts_per_second;
    return result;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR command_line, int command_show)
{
    // Set thread scheduler granularity to 1ms.
    timeBeginPeriod(1u);

    // Register a window class : set of behaviors that several windows might
    // have in common (required even if only a single window is created by the
    // application).
    // CS_VREDRAW | CS_HREDRAW -> Redraw the entire window everytime horizontal
    // / vertical movement or resizing is done.
    // CS_OWNDC is used so that a device context for window can be taken once
    // and reused without releasing it each time.
    WNDCLASSEXW window_class = {0};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = win32_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"CustomWindowClass";

    RegisterClassExW(&window_class);

    // Create the actual window.
    const HWND window = CreateWindowExW(
        0, window_class.lpszClassName, L"prism-engine", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1080, NULL, NULL, instance, NULL);
    ShowWindow(window, SW_SHOWNORMAL);

    if (!window)
    {
        // TODO: Logging.
        return -1;
    }

    const HDC device_context = GetDC(window);

    win32_resize_framebuffer(&g_backbuffer, 1080, 720);

    b32 quit = false;

    // Get the number of counts that occur in a second.
    u64 perf_counter_frequency = win32_get_perf_counter_frequency();

    game_input_t prev_game_input = {0};
    game_input_t current_game_input = {0};

    game_input_t *prev_game_input_ptr = &prev_game_input;
    game_input_t *current_game_input_ptr = &current_game_input;

    game_memory_t game_memory = {0};
    game_memory.permanent_memory_block_size = KILOBYTE(4);
    game_memory.permanent_memory_block =
        VirtualAlloc(0, game_memory.permanent_memory_block_size,
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ASSERT(game_memory.permanent_memory_block);

    f32 delta_time = 0.0f;

    // Code to limit framerate.
    const u32 monitor_refresh_rate = 60;
    const u32 game_update_hz = monitor_refresh_rate / 2;
    const u32 target_ms_per_frame = (u32)(1000.0f / (f32)game_update_hz);

    // Get the current value of performance counter.
    // This can be used to find number of 'counts' per frame. Then, by
    // dividing with perf_counter_frequency, we can find how long it took
    // (in seconds) to render this frame.
    u64 last_counter_value = win32_get_perf_counter_value();

    u64 last_timestamp_value = __rdtsc();

    u32 frame_index = 0;

    while (!quit)
    {
        MSG message = {0};

        ZeroMemory((void *)current_game_input_ptr, sizeof(game_input_t));

        while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
        {
            switch (message.message)
            {
            case WM_QUIT: {
                quit = true;
            }

            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN: {
                b32 is_key_down = ~((message.lParam >> 30) & 0x1);

                switch (message.wParam)
                {
                case VK_ESCAPE: {
                    quit = true;
                }
                break;

                case 'W': {
                    win32_handle_key_input(
                        &current_game_input_ptr->keyboard_state.key_w,
                        is_key_down);
                }
                break;

                case 'S': {
                    win32_handle_key_input(
                        &current_game_input_ptr->keyboard_state.key_s,
                        is_key_down);
                }
                break;

                case 'A': {
                    win32_handle_key_input(
                        &current_game_input_ptr->keyboard_state.key_a,
                        is_key_down);
                }
                break;

                case 'D': {
                    win32_handle_key_input(
                        &current_game_input_ptr->keyboard_state.key_d,
                        is_key_down);
                }
                break;
                }
            }
            break;

            default: {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            break;
            }
        }

        const win32_window_dimensions_t window_dimensions =
            win32_get_window_client_dimensions(window);

        // Render and update the game.
        game_offscreen_buffer_t game_offscreen_buffer = {0};
        game_offscreen_buffer.framebuffer_memory =
            g_backbuffer.framebuffer_memory;
        game_offscreen_buffer.width = g_backbuffer.width;
        game_offscreen_buffer.height = g_backbuffer.height;

        game_input_t game_input = {0};
        game_input.keyboard_state = current_game_input_ptr->keyboard_state;
        game_input.delta_time = delta_time;

        game_input_t *temp = current_game_input_ptr;
        current_game_input_ptr = prev_game_input_ptr;
        prev_game_input_ptr = temp;

        game_update_and_render(&game_offscreen_buffer, &game_input,
                               &game_memory);

        u64 end_counter_value = win32_get_perf_counter_value();

        f32 ms_for_frame = win32_get_time_delta_ms(
            last_counter_value, end_counter_value, perf_counter_frequency);

        if (ms_for_frame <= target_ms_per_frame)
        {
            Sleep((DWORD)(target_ms_per_frame - ms_for_frame));
        }
        else
        {
            // Missed the time within which frame has to be prepared (audio +
            // video).
        }
        end_counter_value = win32_get_perf_counter_value();

        ms_for_frame = win32_get_time_delta_ms(
            last_counter_value, end_counter_value, perf_counter_frequency);

        delta_time = ms_for_frame;

        win32_render_buffer_to_window(&g_backbuffer, device_context,
                                      window_dimensions.width,
                                      window_dimensions.height);

        frame_index++;

        // NOTE: The value of perf counter is fetched right after Sleep is done.
        // It does NOT account for rendering the buffer to window. Should check
        // if RTDSC has to also be fetched after sleep (it isn't being used for
        // now to control framerate).
        u64 end_timestamp_value = __rdtsc();
        u64 clock_cycles_per_frame = end_timestamp_value - last_timestamp_value;

        // 1 frames time -> milli_seconds_for_frame = 1 / seconds in
        // dimension. To find the frames per second, we do -> 1 / time for
        // single frame.
        i32 fps = truncate_f32_to_i32(1000 / delta_time);

        char text[256];
        sprintf(text,
                "MS for frame : %f ms, FPS : %d, Clocks per frame :  %llu \n",
                ms_for_frame, fps, clock_cycles_per_frame);
        OutputDebugStringA(text);

        last_counter_value = end_counter_value;
        last_timestamp_value = end_timestamp_value;
    }

    return 0;
}
