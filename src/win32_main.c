#ifndef UNICODE
#define UNICODE
#endif

#include "common.h"
#include "game.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>

#include <stdio.h>

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

// NOTE: even though bitmap info has the width and the height, because a top
// down framebuffer is used, the height is inverted. Instead of inverting
// height each time it is required, a separate field for height
// is created.
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
    ASSERT(window);

    win32_window_dimensions_t result = {0};

    RECT client_rect = {0};
    GetClientRect(window, &client_rect);

    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

internal void win32_render_buffer_to_window(
    const win32_offscreen_buffer_t *restrict buffer, const HDC device_context,
    const u32 window_width, const u32 window_height)
{
    ASSERT(buffer);

    StretchDIBits(device_context, 0, 0, window_width, window_height, 0, 0,
                  buffer->width, buffer->height, buffer->framebuffer_memory,
                  &buffer->bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

internal void win32_resize_framebuffer(
    win32_offscreen_buffer_t *restrict const buffer, const u32 width,
    const u32 height)
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
    ASSERT(input);

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
        INVALID_CODE_PATH(
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

        // OutputDebugStringW(L"WM_PAINT\n");
    }
    break;

    case WM_CLOSE: {
        DestroyWindow(window);
        // OutputDebugStringW(L"WM_CLOSE\n");
    }
    break;

    case WM_DESTROY: {
        PostQuitMessage(0);
        // OutputDebugStringW(L"WM_DESTROY\n");
    }
    break;

    default: {
        result = DefWindowProc(window, message, w_param, l_param);
    }
    break;
    }

    return result;
}

internal DEF_PLATFORM_READ_FILE_FUNC(platform_read_file)
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

internal DEF_PLATFORM_CLOSE_FILE_FUNC(platform_close_file)
{
    VirtualFree(file_buffer, 0, MEM_RELEASE);
}

internal DEF_PLATFORM_WRITE_TO_FILE_FUNC(platform_write_to_file)
{
    ASSERT(file_name);
    ASSERT(string);

    b32 result = true;

    HANDLE file_handle =
        CreateFileA(file_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);

    if (file_handle != INVALID_HANDLE_VALUE)
    {
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

typedef struct
{
    game_update_and_render_t *update_and_render;
    FILETIME dll_last_write_time;
    HMODULE game_dll;
} game_t;

internal DEF_GAME_UPDATE_AND_RENDER_FUNC(win32_game_update_and_render_stub)
{
    return;
}

void win32_unload_game_dll(game_t *game)
{
    if (game->game_dll)
    {
        FreeLibrary(game->game_dll);

        game->game_dll = NULL;
        game->update_and_render = win32_game_update_and_render_stub;
    }
}

// NOTE: dll_last_write_time has to be filled outside of this function!!!
game_t win32_load_game_dll(const char *game_dll_file_path)
{
    game_t game = {0};
    game.update_and_render = win32_game_update_and_render_stub;

    // NOTE: The platform does not load the game_dll_file_path directly. This is
    // because debuggers will "LOCK" the dll because of which hot reloading
    // won't be possible. To combat this issue, the game_dll_file_path is copied
    // into a temp dll. This temp dll is what is actually loaded.
    if (CopyFileA(game_dll_file_path, "game_temp.dll", FALSE))
    {
        game.game_dll = LoadLibraryA("game_temp.dll");
        if (game.game_dll)
        {
            game.update_and_render = (game_update_and_render_t *)GetProcAddress(
                game.game_dll, "game_update_and_render");
        }
    }
    else
    {
        DWORD error = GetLastError();
        INVALID_CODE_PATH("Failed to copy dll into a temp dll file. This is "
                          "important since debuggers will lock the dll");
    }

    return game;
}

internal FILETIME win32_get_last_write_time(const char *file_name)
{
    FILETIME result = {0};

    WIN32_FIND_DATAA file_data = {0};
    if (FindFirstFileA(file_name, &file_data))
    {
        result = file_data.ftLastWriteTime;
    }

    return result;
}

// State machine for live loop editing.
typedef enum
{
    win32_state_type_none = 0,
    win32_state_type_recording = 1,
    win32_state_type_playback = 2,
} win32_state_type_t;

// Game state is saved in a few files (only for recording and playback state
// types).
typedef struct
{
    HANDLE input_file_handle;
    HANDLE game_memory_file_handle;
} win32_state_t;

win32_state_t win32_start_recording(game_memory_t *const restrict game_memory)
{
    ASSERT(game_memory);

    // Create and open file for game input and game memory.
    HANDLE game_state_handle =
        CreateFileA("prism_game_memory.txt", GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT(game_state_handle != INVALID_HANDLE_VALUE);

    DWORD bytes_written = 0;
    if (!WriteFile(game_state_handle,
                   (void *)game_memory->permanent_memory_block,
                   (DWORD)game_memory->permanent_memory_block_size,
                   &bytes_written, NULL))
    {
        ASSERT(false);
    }

    ASSERT(bytes_written == game_memory->permanent_memory_block_size);

    HANDLE input_file_handle =
        CreateFileA("prism_input_handle.txt", GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    ASSERT(input_file_handle != INVALID_HANDLE_VALUE);

    win32_state_t state = {0};
    state.input_file_handle = input_file_handle;
    state.game_memory_file_handle = game_state_handle;

    return state;
}

void win32_stop_recording(win32_state_t *const restrict state)
{
    ASSERT(state);

    ASSERT(state->input_file_handle != INVALID_HANDLE_VALUE);
    ASSERT(state->game_memory_file_handle != INVALID_HANDLE_VALUE);

    CloseHandle(state->input_file_handle);
    CloseHandle(state->game_memory_file_handle);
}

void win32_start_playback(win32_state_t *const restrict state,
                          game_memory_t *restrict game_memory)
{
    ASSERT(state);
    ASSERT(game_memory);
    ASSERT(game_memory->permanent_memory_block);

    HANDLE game_memory_handle =
        CreateFileA("prism_game_memory.txt", GENERIC_READ, 0, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT(game_memory_handle != INVALID_HANDLE_VALUE);

    HANDLE input_file_handle =
        CreateFileA("prism_input_handle.txt", GENERIC_READ, 0, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    ASSERT(input_file_handle != INVALID_HANDLE_VALUE);

    state->input_file_handle = input_file_handle;
    state->game_memory_file_handle = game_memory_handle;

    DWORD bytes_read = 0;
    // Read input and game state from memory.
    if (!ReadFile(state->game_memory_file_handle,
                  game_memory->permanent_memory_block,
                  truncate_u64_to_u32(game_memory->permanent_memory_block_size),
                  &bytes_read, NULL))
    {
        DWORD error = GetLastError();
        INVALID_CODE_PATH(
            "Failed to read game memory state from file during live loopback");
    }

    ASSERT(bytes_read == game_memory->permanent_memory_block_size);
}

void win32_stop_playback(win32_state_t *const restrict state)
{
    ASSERT(state);

    ASSERT(state->input_file_handle != INVALID_HANDLE_VALUE);
    ASSERT(state->game_memory_file_handle != INVALID_HANDLE_VALUE);

    CloseHandle(state->input_file_handle);
    CloseHandle(state->game_memory_file_handle);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR command_line, int command_show)
{
    win32_state_type_t state_type = 0;
    win32_state_t recording_state = {0};

    // Load the game.
    game_t game = win32_load_game_dll("game.dll");
    game.dll_last_write_time = win32_get_last_write_time("game.dll");

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
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL,
        instance, NULL);
    ShowWindow(window, SW_SHOWNORMAL);

    if (!window)
    {
        // TODO: Logging.
        return -1;
    }

    const HDC device_context = GetDC(window);

    win32_resize_framebuffer(&g_backbuffer, WINDOW_WIDTH, WINDOW_HEIGHT);

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
    u32 monitor_refresh_rate = 60;

    DEVMODEA dev_mode = {};
    if (EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &dev_mode))
    {
        monitor_refresh_rate = dev_mode.dmDisplayFrequency;
    }

    const u32 game_update_hz = monitor_refresh_rate;
    const u32 target_ms_per_frame = (u32)(1000.0f / (f32)game_update_hz);

    // Get the current value of performance counter.
    // This can be used to find number of 'counts' per frame. Then, by
    // dividing with perf_counter_frequency, we can find how long it took
    // (in seconds) to render this frame.
    u64 last_counter_value = win32_get_perf_counter_value();
    u64 last_timestamp_value = __rdtsc();

    u32 frame_index = 0;

    b32 quit = false;
    while (!quit)
    {
        // Check if the game dll's last write time has changed. If yes, re-load
        // the dll.
        FILETIME dll_last_write_time = win32_get_last_write_time("game.dll");

        if (CompareFileTime(&game.dll_last_write_time, &dll_last_write_time) !=
            0)
        {
            win32_unload_game_dll(&game);
            game = win32_load_game_dll("game.dll");
            game.dll_last_write_time = dll_last_write_time;
        }

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

                case 'R': {
                    if (state_type == win32_state_type_none)
                    {
                        state_type = win32_state_type_recording;
                        recording_state = win32_start_recording(&game_memory);
                    }
                    else if (state_type == win32_state_type_playback)
                    {
                        CloseHandle(recording_state.input_file_handle);
                        CloseHandle(recording_state.game_memory_file_handle);

                        state_type = win32_state_type_none;
                    }
                }

                break;

                case 'P': {
                    if (state_type == win32_state_type_recording)
                    {
                        win32_stop_recording(&recording_state);

                        state_type = win32_state_type_playback;

                        win32_start_playback(&recording_state, &game_memory);
                    }
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

        game_platform_services_t platform_services = {0};
        platform_services.read_file = platform_read_file;
        platform_services.write_to_file = platform_write_to_file;
        platform_services.close_file = platform_close_file;

        if (state_type == win32_state_type_recording)
        {
            ASSERT(WriteFile(recording_state.input_file_handle,
                             (void *)&game_input, sizeof(game_input_t), NULL,
                             NULL));
        }

        if (state_type == win32_state_type_playback)
        {
            DWORD bytes_read = 0;

            if (ReadFile(recording_state.input_file_handle, (void *)&game_input,
                         sizeof(game_input_t), &bytes_read, NULL))
            {
                if (bytes_read == 0)
                {
                    win32_stop_playback(&recording_state);
                    win32_start_playback(&recording_state, &game_memory);
                }
            }
        }

        game.update_and_render(&game_offscreen_buffer, &game_input,
                               &game_memory, &platform_services);

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
