#include "win32_main.h"
#include <stdio.h>
#include <timeapi.h>
#include <winnt.h>

// NOTE: Explanation of the rendering logic:
// The engine allocates memory for its own bitmap and renders into it. GDI's
// (graphics device interface) role is to simply 'Blit' or copy the rendered
// bitmap into the actual device context.
// The bitmap will be of fixed size and will not depend on window resolution.

global_variable win32_framebuffer_t g_offscreen_framebuffer;

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
    b32 is_key_down =
        current_keyboard_state->key_states[virtual_keycode] & 0x80;

    b32 was_key_down =
        previous_keyboard_state->key_states[virtual_keycode] & 0x80;

    game_key_state_t key_state = {0};
    key_state.is_key_down = is_key_down;
    key_state.state_changed = (is_key_down != was_key_down);

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
    ASSERT(file_content_buffer != NULL);
    VirtualFree(file_content_buffer, 0, MEM_RELEASE);
}

internal void platform_write_to_file(const char *restrict file_path,
                                     u8 *restrict file_content_buffer,
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

internal FUNC_GAME_RENDER(stub_game_render)
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

    WIN32_FILE_ATTRIBUTE_DATA file_attribute_data = {0};
    GetFileAttributesExA(file_path, GetFileExInfoStandard,
                         &file_attribute_data);

    last_time_file_was_modified = file_attribute_data.ftLastWriteTime;

    return last_time_file_was_modified;
}

LRESULT CALLBACK win32_window_proc(HWND window_handle, UINT message,
                                   WPARAM wparam, LPARAM lparam);

internal void win32_start_state_recording(win32_state_t *restrict win32_state,
                                          u8 *restrict game_permanent_memory)
{
    // Open the file handle. KEEP it opened, so we can continuously write to
    // file and append data to it.
    win32_state->input_recording_file_handle =
        CreateFileA("prism-state.sta", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);

    CopyMemory(win32_state->game_memory, game_permanent_memory,
               win32_state->game_memory_size);
}

internal void win32_record_state(win32_state_t *restrict win32_state,
                                 game_input_t *restrict game_input)
{
    // Write the game input into the file denoted by
    // input_recording_file_handle.
    DWORD number_of_bytes_written = {0};
    WriteFile(win32_state->input_recording_file_handle, game_input,
              sizeof(game_input_t), &number_of_bytes_written, NULL);
}

internal void win32_stop_state_recording(win32_state_t *restrict win32_state,
                                         game_input_t *restrict game_input)
{
    CloseHandle(win32_state->input_recording_file_handle);
}

internal void win32_start_playback(win32_state_t *restrict win32_state,
                                   game_input_t *restrict game_input,
                                   u8 *restrict permanent_game_memory_pointer)
{
    // Keep reading from file, but if number of bytes read is 0, go back to the
    // start.
    if (!win32_state->input_playback_file_handle)
    {
        win32_state->input_playback_file_handle =
            CreateFileA("prism-state.sta", GENERIC_READ, 0, NULL, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);

        // When you start playback, permanent game memory pointer is set to the
        // win32 state pointer.
        CopyMemory(permanent_game_memory_pointer, win32_state->game_memory,
                   win32_state->game_memory_size);
    }

    // Read from file.
    DWORD number_of_bytes_read = {0};
    ReadFile(win32_state->input_playback_file_handle, game_input,
             sizeof(game_input_t), &number_of_bytes_read, NULL);

    if (number_of_bytes_read == 0)
    {
        CloseHandle(win32_state->input_playback_file_handle);
        win32_state->input_playback_file_handle = NULL;
        win32_start_playback(win32_state, game_input,
                             permanent_game_memory_pointer);
    }
}

internal void win32_stop_playback(win32_state_t *win32_state)
{
    CloseHandle(win32_state->input_playback_file_handle);
}

internal SIZE_T win32_setup_large_pages_and_get_minimum_lage_page_size()
{
    HANDLE token = {0};
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
    {
        ASSERT(0);
    }

    // Lookup the LUID of the SeLockMemoryPrivilege.
    LUID se_lock_memory_privilege_luid = {0};
    if (!LookupPrivilegeValueA(NULL, SE_LOCK_MEMORY_NAME,
                               &se_lock_memory_privilege_luid))
    {
        ASSERT(0);
    }

    LUID_AND_ATTRIBUTES se_lock_memory_privilege_and_attributes = {0};
    se_lock_memory_privilege_and_attributes.Luid =
        se_lock_memory_privilege_luid;
    se_lock_memory_privilege_and_attributes.Attributes = SE_PRIVILEGE_ENABLED;

    TOKEN_PRIVILEGES token_privileges = {0};
    token_privileges.Privileges[0] = se_lock_memory_privilege_and_attributes;
    token_privileges.PrivilegeCount = 1;

    if (!AdjustTokenPrivileges(token, FALSE, &token_privileges, 0, NULL, NULL))
    {
        ASSERT(0);
    }

    DWORD adjust_token_privilege_error = GetLastError();
    if (adjust_token_privilege_error == ERROR_NOT_ALL_ASSIGNED)
    {
        MessageBoxA(
            NULL,
            "AdjustTokenPrivileges was succesfull, but SeLockMemoryPrivilege "
            "could not be set. Consider opening 'Local Security Policy/Local "
            "Policies/User Rights Assignment/Lock pages in memory' to find if "
            "your user falls under the 'security setting' tab",
            "ERROR", MB_OK);
    }

    return GetLargePageMinimum();
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance,
                   LPSTR command_line, int show_command)
{
    // If memory is allocated with the large page flag, the alignment and size
    // must be a multiple of the minimum large page size.
    SIZE_T win32_minimum_large_page_size =
        win32_setup_large_pages_and_get_minimum_lage_page_size();

    // Set the windows schedular granularity level.
    const u32 windows_schedular_granularity_level = 1u;
    b32 is_schedular_granularity_per_ms =
        (b32)(timeBeginPeriod(windows_schedular_granularity_level) ==
              TIMERR_NOERROR);

    // Get the number of increments / counts the high performance counter
    // does in a single second. According to MSDN, the high perf counter has
    // granularity in the < 1 micro second space. The number of counter
    // increments per second is fixed, and can be fetched at application
    // startup alone.
    u64 perf_counter_frequency = win32_query_perf_frequency();

    // Create the window class, which defines a set of behaviours that
    // multiple windows might have in common.
    // device context can be fetched once and reused multiple times.
    WNDCLASSA window_class = {0};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = win32_window_proc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance;
    window_class.lpszClassName = "BaseWindowClass";

    if (!RegisterClassA(&window_class))
    {
        OutputDebugStringA("Failed to create window class.");
    }

    // TODO: having the WS_EX_TOPMOST window flag causes some issues because
    // while the window is transparent, if the mouse is over the top most window
    // this window captures it, which is annoying when debugging happens. Create
    // a window.
    HWND window_handle =
        CreateWindowExA(/*WS_EX_TOPMOST |*/ WS_EX_LAYERED, "BaseWindowClass",
                        "prism-engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 1920, 1080, NULL, NULL, instance, NULL);
    if (window_handle == NULL)
    {
        OutputDebugStringA("Failed to create window.");
    }

    ShowWindow(window_handle, SW_SHOW);

    win32_resize_bitmap(1920 / 4, 1080 / 4);

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

    // Allocate memory upfront.
    win32_memory_allocator_t memory_allocator = {0};
    memory_allocator.permanent_memory_size =
        get_nearest_multiple(MEGABYTE(1), win32_minimum_large_page_size);
    memory_allocator.permanent_memory = VirtualAlloc(
        NULL, memory_allocator.permanent_memory_size,
        MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
    DWORD error = GetLastError();
    if (error == 1450)
    {
        MessageBoxA(NULL,
                    "Game cannot start as system is out of resources "
                    "[memory_allocator could not allocate 1MB]",
                    "ERROR", MB_OK);
    }

    ASSERT(memory_allocator.permanent_memory != NULL);

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

    // Setup the win32 state and allocate memory for the game permanent pointer.
    win32_state_t win32_state = {0};
    win32_state.current_state = WIN32_STATE_NONE;
    win32_state.game_memory = VirtualAlloc(
        NULL, memory_allocator.permanent_memory_size,
        MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
    win32_state.game_memory_size = memory_allocator.permanent_memory_size;

    // Find the ms it takes to update the backbuffer.
    // NOTE: This will wary each time the window is resized!!!
    // The fix for this is a bit hacky, but each time the window is resized
    // the stretch dibits call is profiled and the
    // backbuffer_update_time_seconds is updated.
    win32_dimensions_t client_dimensions =
        win32_get_client_region_dimensions(window_handle);

    HDC window_device_context = GetDC(window_handle);

    u64 backbuffer_update_start_time = win32_query_perf_counter();
    win32_update_backbuffer(window_device_context, client_dimensions.width,
                            client_dimensions.height);

    f32 backbuffer_update_time_seconds = (win32_get_seconds_elapsed(
        perf_counter_frequency, backbuffer_update_start_time,
        win32_query_perf_counter()));

    // Before releasing the DC, get the monitor refresh rate.
    u64 monitor_refresh_rate = 60;

    i32 queried_refresh_rate = GetDeviceCaps(window_device_context, VREFRESH);

    if (queried_refresh_rate > 1)
    {
        monitor_refresh_rate = queried_refresh_rate;
    }
    const u64 game_update_hz = monitor_refresh_rate;
    const f32 target_seconds_per_frame = 1.0f / game_update_hz;
    ReleaseDC(window_handle, window_device_context);

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
        window_device_context = GetDC(window_handle);

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
        game_input.dt_for_frame = target_seconds_per_frame;

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

        game_key_state_t r_key_state = get_game_key_state(
            current_keyboard_state_ptr, previous_keyboard_state_ptr, 'R');

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

        // If the R key (for record) is pressed, recording starts.
        if (r_key_state.is_key_down && r_key_state.state_changed)
        {
            if (win32_state.current_state == WIN32_STATE_NONE)
            {
                win32_state.current_state = WIN32_STATE_RECORDING;

                win32_start_state_recording(
                    &win32_state, game_memory_allocator.permanent_memory);
            }
            else if (win32_state.current_state == WIN32_STATE_RECORDING)
            {
                win32_stop_state_recording(&win32_state, &game_input);
                win32_state.current_state = WIN32_STATE_PLAYBACK;
            }
            else if (win32_state.current_state == WIN32_STATE_PLAYBACK)
            {
                // Stop play black.
                win32_stop_playback(&win32_state);
                win32_state.current_state = WIN32_STATE_NONE;
            }
        }

        if (win32_state.current_state == WIN32_STATE_RECORDING)
        {
            win32_record_state(&win32_state, &game_input);
        }
        else if (win32_state.current_state == WIN32_STATE_PLAYBACK)
        {
            win32_start_playback(&win32_state, &game_input,
                                 memory_allocator.permanent_memory);
        }
        game_code.game_render(&game_memory_allocator, &game_framebuffer,
                              &game_input, &platform_services);

        win32_dimensions_t new_client_dimensions =
            win32_get_client_region_dimensions(window_handle);

        // Sleep so that we can display the rendered frame as close as
        // possible to the boundary of the vertical blank.
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
            while (seconds_elapsed_for_processing_frame +
                       backbuffer_update_time_seconds <
                   target_seconds_per_frame)
            {
                seconds_elapsed_for_processing_frame =
                    win32_get_seconds_elapsed(perf_counter_frequency,
                                              last_counter_value,
                                              win32_query_perf_counter());
            }
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

        ReleaseDC(window_handle, window_device_context);
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
    // WM_ACTIVATEAPP can be used to figure out if current window is active.
    // If it is active, window will be opaque (since that is the center of
    // focus). However, if its now, set the window opacity to 50% (make it a
    // bit transclusent).
    case WM_ACTIVATEAPP: {
        BOOL is_window_active = (BOOL)wparam;
        if (!is_window_active)
        {
            SetLayeredWindowAttributes(window_handle, RGB(0, 0, 0), 64,
                                       LWA_ALPHA);
        }
        else
        {
            SetLayeredWindowAttributes(window_handle, RGB(0, 0, 0), 255,
                                       LWA_ALPHA);
        }
    }
    break;

        // WM_CLOSE is called when the window is closed
        // (shortcut key or the X button).
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
        const b32 is_key_down = (lparam & (1 << 31)) == 0;

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
