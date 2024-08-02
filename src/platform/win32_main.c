#include "win32_main.h"

#include <fileapi.h>
#include <libloaderapi.h>
#include <stdio.h>
#include <timeapi.h>
#include <winbase.h>

// NOTE: Explanation of the rendering logic:
// The engine allocates memory for its own bitmap and renders into it. GDI's
// (graphics device interface) role is to simply 'Blit' or copy the rendered
// bitmap into the actual device context.
// The bitmap will be of fixed size and will not depend on window resolution.

global_variable win32_offscreen_framebuffer_t g_offscreen_framebuffer;

internal win32_dimensions_t
win32_get_client_region_dimensions(const HWND window_handle)
{
    RECT client_rect = {0};
    GetClientRect(window_handle, &client_rect);

    win32_dimensions_t dimensions = {0};
    dimensions.width = client_rect.right - client_rect.left;
    dimensions.height = client_rect.bottom - client_rect.top;

    return dimensions;
}

// Function to create / resize the bitmap info header and memory for backbuffer.
// NOTE: For now, the bitmap does not change its dimensions if the client region
// is resized. This is because stretch DIBits will take care of situations where
// source and destination dimensions (for blitting) is different.
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
    ASSERT(g_offscreen_framebuffer.backbuffer_memory != NULL);

    // Setup the bitmap info struct

    // NOTE: the negative height, this is to ensure that the bitmap is
    // a top down DIB.
    g_offscreen_framebuffer.bitmap_info_header.biSize =
        sizeof(BITMAPINFOHEADER);
    g_offscreen_framebuffer.bitmap_info_header.biWidth = width;
    g_offscreen_framebuffer.bitmap_info_header.biHeight = -1 * height;
    g_offscreen_framebuffer.bitmap_info_header.biPlanes = 1;
    g_offscreen_framebuffer.bitmap_info_header.biBitCount = 32;
    g_offscreen_framebuffer.bitmap_info_header.biCompression = BI_RGB;
    g_offscreen_framebuffer.bitmap_info_header.biSizeImage = 0;
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

internal u64 win32_query_perf_frequency()
{
    LARGE_INTEGER frequency_value = {0};
    QueryPerformanceFrequency(&frequency_value);

    return frequency_value.QuadPart;
}

internal u64 win32_query_perf_counter()
{
    LARGE_INTEGER counter_value = {0};
    QueryPerformanceCounter(&counter_value);

    return counter_value.QuadPart;
}

internal f32 win32_get_seconds_elapsed(u64 performance_frequency,
                                       u64 start_counter_value,
                                       u64 end_counter_value)
{
    f32 seconds_elapsed = (f32)(end_counter_value - start_counter_value) /
                          (f32)performance_frequency;
    return seconds_elapsed;
}

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
    key_state.state_changed = (is_key_down != was_key_down);

    if (key_state.state_changed)
    {
        int x = 3;
    }

    return key_state;
}

// Services / platform provided by the platform layer to the game.
internal platform_file_read_result_t
platform_read_entire_file(const char *file_path)
{
    platform_file_read_result_t file_read_result = {0};

    HANDLE file_handle =
        CreateFileA(file_path, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER file_size = {0};
        if (GetFileSizeEx(file_handle, &file_size))
        {
            // Once we have the file size, allocate enough memory so file
            // contents can be moved to the buffer.

            file_read_result.file_content_buffer =
                VirtualAlloc(NULL, file_size.QuadPart, MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
            u32 number_of_bytes_read_from_file = 0;
            if (ReadFile(file_handle, file_read_result.file_content_buffer,
                         (u32)file_size.QuadPart,
                         (LPDWORD)&number_of_bytes_read_from_file, NULL) &&
                number_of_bytes_read_from_file == file_size.QuadPart)
            {
                // File was read succesfully.
                file_read_result.file_content_size =
                    number_of_bytes_read_from_file;
            }
            else
            {
                // Failed to read from file.
                // Free the allocated memory immediately.
                VirtualFree(file_read_result.file_content_buffer, 0,
                            MEM_RELEASE);
            }
        }
        else
        {
            // Failed to get file size.
        }

        CloseHandle(file_handle);
    }
    else
    {
        // Could not open the file.
    }

    return file_read_result;
}

internal void platform_close_file(u8 *file_content_buffer)
{
    VirtualFree(file_content_buffer, 0, MEM_RELEASE);
}

internal void platform_write_to_file(const char *file_path,
                                     u8 *file_content_buffer,
                                     u64 file_content_size)
{
    ASSERT(file_content_size != 0);

    HANDLE file_handle =
        CreateFileA(file_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        u32 number_of_bytes_written = 0;
        if (WriteFile(file_handle, file_content_buffer, (u32)file_content_size,
                      (LPDWORD)&number_of_bytes_written, NULL) &&
            (file_content_size == number_of_bytes_written))
        {
            // Succesfully wrote to file!
        }
        else
        {
        }
    }
    else
    {
    }

    CloseHandle(file_handle);
}

FUNC_GAME_RENDER(stub_game_render)
{
    return;
}

// NOTE: The last dll last write time is NOT set in this function. Do it before
// this func is called!
internal game_code_t win32_load_game_dll(const char *file_path)
{
    // We cant use load library directly on file_path. This is because debuggers
    // (like VS) will lock the DLL, which means that we cannot recompile the DLL
    // again (with same name). The solution for now is to Copy the DLL, and use
    // the duplicate DLL in LoadLibrary.

    game_code_t game_code = {0};
    game_code.game_render = stub_game_render;

    if (CopyFile(file_path, "game_duplicate.dll", FALSE))
    {
        game_code.game_dll_module = LoadLibraryA("game_duplicate.dll");
        if (game_code.game_dll_module)
        {
            game_code.game_render = (game_render_t *)GetProcAddress(
                game_code.game_dll_module, "game_render");
        }
        else
        {
            ASSERT(0);
        }
    }
    else
    {
        // Copy file should never fail!!
        // ASSERT(0);
    }

    return game_code;
}

internal void win32_unload_game_dll(game_code_t *game_code)
{
    if (game_code->game_dll_module)
    {
        FreeLibrary(game_code->game_dll_module);
        game_code->game_dll_module = NULL;
        game_code->game_render = stub_game_render;
    }
}

internal FILETIME win32_get_last_time_file_was_modified(const char *file_path)
{
    FILETIME last_time_file_was_modified = {0};

    WIN32_FIND_DATAA file_find_data = {0};
    HANDLE dll_handle = FindFirstFileA(file_path, &file_find_data);
    if (dll_handle != INVALID_HANDLE_VALUE)
    {
        last_time_file_was_modified = file_find_data.ftLastWriteTime;
        FindClose(dll_handle);
    }
    else
    {
        ASSERT(0);
    }

    return last_time_file_was_modified;
}

LRESULT CALLBACK win32_window_proc(HWND window_handle, UINT message,
                                   WPARAM wparam, LPARAM lparam);

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance,
                   LPSTR command_line, int show_command)
{
    // Set the windows schedular granularity level.
    const u32 windows_schedular_granularity_level = 1u;
    b8 is_schedular_granularity_per_ms =
        (b8)(timeBeginPeriod(windows_schedular_granularity_level) ==
             TIMERR_NOERROR);

    // FIX: Is there some windows function to get the actual monitor
    // refresh rate rather than just assuming it to be 60?
    const u64 monitor_refresh_rate = 60;
    const u64 game_update_hz = monitor_refresh_rate;
    const f32 target_seconds_per_frame = 1.0f / game_update_hz;

    // Get the number of increments / counts the high performance counter
    // does in a single second. According to MSDN, the high perf counter has
    // granularity in the < 1 micro second space. The number of counter
    // increments per second is fixed, and can be fetched at application
    // startup alone.
    u64 perf_counter_frequency = win32_query_perf_frequency();

    // Create the window class, which defines a set of behaviours that
    // multiple windows might have in common. Since CS_OWNDC is used, the
    // device context can be fetched once and reused multiple times.
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

    // RDTSC stands for read timestamp counter. Each processes will have a
    // time stamp counter, which basically increments after each clock
    // cycle. The
    // __rdtsc call is a actually not a function call, but a intrinsic,
    // which tells the compiler to insert a assembly language call in place
    // of the function definition. For example, when the dis-assembly is
    // examined, the rdtsc x86 call is inserted rather than a function call.
    // This can cause some problems when instruction reordering happens,
    // since the asm call may not be reordered.
    u64 end_timestamp_counter = __rdtsc();

    // Some explanation for the input module :
    // To determine if a key state has changed, the platform layer will
    // store 2 copies of input data structures, which are swapped each frame
    // . Finding state change becomes trivial in this case.
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
    ASSERT(memory_allocator.permanent_memory != NULL);

    // Find the ms it takes to update the backbuffer.
    // NOTE: This will wary each time the window is resized!!!
    // The fix for this is a bit hacky, but each time the window is resized the
    // stretch dibits call is profiled and the backbuffer_update_time_seconds is
    // updated.
    win32_dimensions_t client_dimensions =
        win32_get_client_region_dimensions(window_handle);

    u64 backbuffer_update_start_time = win32_query_perf_counter();
    win32_update_backbuffer(window_device_context, client_dimensions.width,
                            client_dimensions.height);
    f32 backbuffer_update_time_seconds = (win32_get_seconds_elapsed(
        perf_counter_frequency, backbuffer_update_start_time,
        win32_query_perf_counter()));

    u64 last_counter_value = win32_query_perf_counter();

    game_code_t game_code = win32_load_game_dll("game.dll");
    // Get the last time file was modified.
    game_code.dll_last_modification_time =
        win32_get_last_time_file_was_modified("game.dll");

    platform_services_t platform_services = {0};
    platform_services.platform_read_entire_file = platform_read_entire_file;
    platform_services.platform_close_file = platform_close_file;
    platform_services.platform_write_to_file = platform_write_to_file;

    // Main game loop.
    while (1)
    {
        // Check the last modified time of the DLL.
        FILETIME dll_last_modified_time =
            win32_get_last_time_file_was_modified("game.dll");
        if (CompareFileTime(&dll_last_modified_time,
                            &game_code.dll_last_modification_time) != 0)
        {
            win32_unload_game_dll(&game_code);
            game_code = win32_load_game_dll("game.dll");

            game_code.dll_last_modification_time.dwLowDateTime =
                dll_last_modified_time.dwLowDateTime;

            game_code.dll_last_modification_time.dwHighDateTime =
                dll_last_modified_time.dwHighDateTime;
        }

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
        GetKeyboardState(current_keyboard_state_ptr->key_states);

        game_input_t game_input = {0};
        game_input.keyboard_state.key_w = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'W');

        game_input.keyboard_state.key_s = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'S');

        game_input.keyboard_state.key_a = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'A');

        game_input.keyboard_state.key_d = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'D');

        game_input.keyboard_state.key_space = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, VK_SPACE);

        // NOTE: Should rendering only be done when WM_PAINT is
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

        game_code.game_render(&game_memory_allocator, &game_framebuffer,
                              &game_input, &platform_services);

        win32_dimensions_t new_client_dimensions =
            win32_get_client_region_dimensions(window_handle);

        // Sleep so that we can display the rendered frame as close as possible
        // to the boundary of the vertical blank.
        u64 counter_value_after_update_and_render = win32_query_perf_counter();

        f32 seconds_elapsed_for_processing_frame = win32_get_seconds_elapsed(
            perf_counter_frequency, last_counter_value,
            counter_value_after_update_and_render);

        if (is_schedular_granularity_per_ms)
        {
            if (seconds_elapsed_for_processing_frame +
                    backbuffer_update_time_seconds <
                target_seconds_per_frame)
            {
                DWORD ms_to_sleep_for =
                    (DWORD)((target_seconds_per_frame -
                             seconds_elapsed_for_processing_frame -
                             backbuffer_update_time_seconds) *
                            1000.0f);
                Sleep(ms_to_sleep_for);
            }
        }
        else
        {
            // TODO: SPIN LOCK?
        }

        // Check if the new client dimensions match the previous one.
        if (new_client_dimensions.width != client_dimensions.width ||
            new_client_dimensions.height != client_dimensions.height)
        {
            const u64 start_backbuffer_update_counter =
                win32_query_perf_counter();
            win32_update_backbuffer(window_device_context,
                                    new_client_dimensions.width,
                                    new_client_dimensions.height);

            backbuffer_update_time_seconds = win32_get_seconds_elapsed(
                perf_counter_frequency, start_backbuffer_update_counter,
                win32_query_perf_counter());

            client_dimensions = new_client_dimensions;
        }
        else
        {
            win32_update_backbuffer(window_device_context,
                                    new_client_dimensions.width,
                                    new_client_dimensions.height);
        }

        // Swap keyboard states.
        {
            win32_keyboard_state_t *temp = current_keyboard_state_ptr;
            current_keyboard_state_ptr = previous_keyboard_state_ptr;
            previous_keyboard_state_ptr = temp;
        }

        u64 counter_value = win32_query_perf_counter();

        f32 ms_per_frame =
            win32_get_seconds_elapsed(perf_counter_frequency,
                                      last_counter_value, counter_value) *
            1000.0f;

        char ms_per_frame_str_buffer[256];
        sprintf_s(&ms_per_frame_str_buffer[0], 256, "MS Per Frame : %f \n",
                  ms_per_frame);

        OutputDebugStringA(ms_per_frame_str_buffer);
        last_counter_value = counter_value;

        u64 current_timestamp_counter = __rdtsc();
        u64 timestamp_difference =
            current_timestamp_counter - end_timestamp_counter;

        end_timestamp_counter = current_timestamp_counter;
    }

    ReleaseDC(window_handle, window_device_context);
    VirtualFree(memory_allocator.permanent_memory, 0, MEM_RELEASE);

    return 0;
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
            win32_get_client_region_dimensions(window_handle);
        HDC handle_to_device_context = BeginPaint(window_handle, &ps);
        win32_update_backbuffer(handle_to_device_context,
                                client_dimensions.width,
                                client_dimensions.height);

        EndPaint(window_handle, &ps);
    }
    break;

    case WM_KEYDOWN:
    case WM_KEYUP: {
        // NOTE: the 31st bit of lparam is 0 for WM_KEYDOWN and 1 for
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
