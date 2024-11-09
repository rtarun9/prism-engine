#ifndef UNICODE
#define UNICODE
#endif

#include "common.h"

#include "game.c"
#include "game.h"

#include <Windows.h>
#include <dsound.h>
#include <xinput.h>

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
global_variable LPDIRECTSOUNDBUFFER g_secondary_buffer = NULL;

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

#define DEF_XINPUT_GET_STATE(name)                                             \
    DWORD WINAPI name(DWORD dw_user_index, XINPUT_STATE *state)
#define DEF_XINPUT_SET_STATE(name)                                             \
    DWORD WINAPI name(DWORD dw_user_index, XINPUT_VIBRATION *vibration)

DEF_XINPUT_GET_STATE(xinput_get_state_stub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

DEF_XINPUT_SET_STATE(xinput_set_state_stub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

typedef DEF_XINPUT_GET_STATE(xinput_get_state_t);
typedef DEF_XINPUT_SET_STATE(xinput_set_state_t);

internal xinput_get_state_t *xinput_get_state = xinput_get_state_stub;
internal xinput_set_state_t *xinput_set_state = xinput_set_state_stub;

internal void win32_load_xinput_functions()
{
    HMODULE xinput_dll = LoadLibraryW(L"xinput1_4.dll");
    if (!xinput_dll)
    {
        xinput_dll = LoadLibraryW(L"xinput1_3.dll");
    }

    if (xinput_dll)
    {
        xinput_get_state =
            (xinput_get_state_t *)GetProcAddress(xinput_dll, "XInputGetState");
        xinput_set_state =
            (xinput_set_state_t *)GetProcAddress(xinput_dll, "XInputSetState");
    }
}

#define DEF_DSOUND_CREATE(name)                                                \
    HRESULT WINAPI name(LPCGUID guid_device,                                   \
                        LPDIRECTSOUND *direct_sound_object,                    \
                        LPUNKNOWN unk_outer_ptr)
typedef DEF_DSOUND_CREATE(dsound_create_t);

DEF_DSOUND_CREATE(dsound_create_stub)
{
    return 0;
}

internal dsound_create_t *dsound_create = dsound_create_stub;

internal void win32_init_dsound(const HWND window,
                                const u32 secondary_buffer_size,
                                const u32 samples_per_second)
{
    const HMODULE dsound_dll = LoadLibraryW(L"dsound.dll");
    if (dsound_dll)
    {
        // Load the required functions to create a dsound object.
        dsound_create =
            (dsound_create_t *)GetProcAddress(dsound_dll, "DirectSoundCreate");

        LPDIRECTSOUND direct_sound = NULL;
        if (dsound_create && SUCCEEDED(dsound_create(0, &direct_sound, 0)))
        {
            // Set the cooperative level. DSSCL_PRIORITY must be used if the
            // application wants to call the SetFormat function.
            if (SUCCEEDED(IDirectSound_SetCooperativeLevel(direct_sound, window,
                                                           DSSCL_PRIORITY)))
            {
                // Create a primary buffer. This is a relic to the old days,
                // where you basically wrote directly to the sound card.
                // It is created just to make sure that audio is not up/down
                // sampled by the system.
                LPDIRECTSOUNDBUFFER primary_buffer = NULL;

                DSBUFFERDESC primary_buffer_desc = {0};
                primary_buffer_desc.dwSize = sizeof(DSBUFFERDESC);
                primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                if (SUCCEEDED(IDirectSound_CreateSoundBuffer(
                        direct_sound, &primary_buffer_desc, &primary_buffer,
                        NULL)))

                {
                    // Set the wave format using the primary buffer.
                    // Use the Pulse code modulation (PCM) format as its the
                    // encoding format used for uncompressed digital audio.
                    // 2 channels are used as we want stereo audio.
                    WAVEFORMATEX wave_format = {0};
                    wave_format.wFormatTag = WAVE_FORMAT_PCM;
                    wave_format.nChannels = 2;
                    wave_format.nSamplesPerSec = samples_per_second;
                    wave_format.wBitsPerSample = 16;
                    wave_format.nBlockAlign =
                        (wave_format.nChannels * wave_format.wBitsPerSample) /
                        8;
                    wave_format.nAvgBytesPerSec =
                        wave_format.nSamplesPerSec * wave_format.nBlockAlign;
                    if (SUCCEEDED(IDirectSoundBuffer_SetFormat(primary_buffer,
                                                               &wave_format)))
                    {
                        // Create the secondary buffer, which is the buffer data
                        // is actually written to.
                        DSBUFFERDESC secondary_buffer_desc = {0};
                        secondary_buffer_desc.dwSize = sizeof(DSBUFFERDESC);
                        secondary_buffer_desc.dwBufferBytes =
                            secondary_buffer_size;
                        secondary_buffer_desc.lpwfxFormat = &wave_format;

                        if (SUCCEEDED(IDirectSound_CreateSoundBuffer(
                                direct_sound, &secondary_buffer_desc,
                                &g_secondary_buffer, NULL)))
                        {
                            OutputDebugStringW(L"Created secondary buffer.\n");
                        }
                    }
                }
            }
        }
    }
}

typedef struct
{
    // bytes per sample is the number of bytes for left channel + right channel.
    u32 bytes_per_sample;
    u32 samples_per_second;
    u32 secondary_buffer_size;

    // A monotonically increasing value that represents the index of audio
    // sample outputted.
    u32 running_sample_index;

    // To reduce latency, each frame only a certain number of samples are set.
    u32 samples_to_write_per_frame;
} win32_sound_output_t;

internal void win32_clear_sound_buffer(win32_sound_output_t *sound_output)
{
    void *region_0 = 0;
    DWORD region_0_bytes = 0;

    DWORD unused_region_1_bytes = 0;
    if (SUCCEEDED(IDirectSoundBuffer_Lock(
            g_secondary_buffer, 0, sound_output->secondary_buffer_size,
            &region_0, &region_0_bytes, NULL, NULL, 0)))
    {
        i16 *sample_region_0 = (i16 *)region_0;

        DWORD region_0_sample_count =
            region_0_bytes / sound_output->bytes_per_sample;

        for (DWORD sample_count = 0; sample_count < region_0_sample_count;
             sample_count++)
        {
            *sample_region_0++ = 0;
            *sample_region_0++ = 0;
        }

        IDirectSoundBuffer_Unlock(g_secondary_buffer, region_0, region_0_bytes,
                                  NULL, unused_region_1_bytes);
    }
}

internal void win32_fill_sound_buffer(win32_sound_output_t *sound_output,
                                      u32 write_cursor_lock_offset,
                                      u32 bytes_to_write,
                                      i16 *game_sound_buffer_memory)
{
    ASSERT(game_sound_buffer_memory);
    ASSERT(sound_output);

    void *region_0 = 0;
    DWORD region_0_bytes = 0;

    void *region_1 = 0;
    DWORD region_1_bytes = 0;

    if (SUCCEEDED(IDirectSoundBuffer_Lock(
            g_secondary_buffer, write_cursor_lock_offset, bytes_to_write,
            &region_0, &region_0_bytes, &region_1, &region_1_bytes, 0)))
    {
        i16 *game_sound_buffer_region = game_sound_buffer_memory;

        i16 *sample_region_0 = (i16 *)region_0;

        DWORD region_0_sample_count =
            region_0_bytes / sound_output->bytes_per_sample;

        for (DWORD sample_count = 0; sample_count < region_0_sample_count;
             sample_count++)
        {
            // TODO: Too many asserts.
            ASSERT(sample_region_0);
            ASSERT(game_sound_buffer_region);

            // Assigning audio sample value for left and right channel.
            *sample_region_0 = *game_sound_buffer_region;
            sample_region_0++;
            game_sound_buffer_region++;

            ASSERT(sample_region_0);
            ASSERT(game_sound_buffer_region);

            *sample_region_0 = *game_sound_buffer_region;

            sample_region_0++;
            game_sound_buffer_region++;

            sound_output->running_sample_index++;
        }

        i16 *sample_region_1 = (i16 *)region_1;

        DWORD region_1_sample_count =
            region_1_bytes / sound_output->bytes_per_sample;

        for (DWORD sample_count = 0; sample_count < region_1_sample_count;
             sample_count++)
        {
            ASSERT(sample_region_1);
            ASSERT(game_sound_buffer_region);

            *sample_region_1++ = *game_sound_buffer_region++;

            ASSERT(sample_region_1);
            ASSERT(game_sound_buffer_region);

            *sample_region_1++ = *game_sound_buffer_region++;

            sound_output->running_sample_index++;
        }

        IDirectSoundBuffer_Unlock(g_secondary_buffer, region_0, region_0_bytes,
                                  region_1, region_1_bytes);
    }
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

internal void win32_draw_line(
    u32 x_offset, u32 y_offset, u32 x_width, u32 y_width,
    win32_offscreen_buffer_t *const restrict backbuffer, u32 color)
{
    u32 *row = backbuffer->framebuffer_memory + x_offset +
               (y_offset + y_width) * backbuffer->width;
    u32 stride = backbuffer->width;

    for (u32 y = y_offset; y < y_offset + y_width; y++)
    {
        u32 *pixel = row;
        for (u32 x = x_offset; x < x_offset + x_width; x++)
        {
            *pixel++ = color;
        }

        row += stride;
    }
}

internal void win32_debug_play_cursor_position(
    u32 *const restrict play_cursor_positions, u32 play_cursor_position_size,
    win32_offscreen_buffer_t *const restrict backbuffer,
    win32_sound_output_t *const restrict sound_output, u32 color)
{
    ASSERT(backbuffer);
    ASSERT(sound_output);
    ASSERT(play_cursor_positions);

    // The backbuffer and play cursor position size are not the same, so the
    // 'offset' and 'padding' has to be computed.
    u32 x_pad = 16;
    u32 y_pad = 16;

    f32 c = play_cursor_position_size / (f32)(backbuffer->width - 2 * x_pad);

    for (u32 i = 0; i < play_cursor_position_size; i++)
    {
        u32 x_offset = (u32)(c * play_cursor_positions[i]);

        win32_draw_line(x_offset, y_pad, 1, 256, backbuffer, color);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR command_line, int command_show)
{
    // Set thread scheduler granularity to 1ms.
    timeBeginPeriod(1u);

    win32_load_xinput_functions();

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

    win32_sound_output_t sound_output = {0};
    sound_output.bytes_per_sample = sizeof(i16) * 2;
    sound_output.samples_per_second = 48000;
    sound_output.secondary_buffer_size =
        sound_output.samples_per_second * sound_output.bytes_per_sample;
    sound_output.running_sample_index = 0;
    sound_output.samples_to_write_per_frame =
        sound_output.samples_per_second / 60;

    win32_init_dsound(window, sound_output.secondary_buffer_size,
                      sound_output.samples_per_second);

    i16 *game_sound_buffer_memory =
        (i16 *)VirtualAlloc(0, sound_output.secondary_buffer_size,
                            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ASSERT(game_sound_buffer_memory);
    win32_clear_sound_buffer(&sound_output);
    IDirectSoundBuffer_Play(g_secondary_buffer, 0, 0, DSBPLAY_LOOPING);

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
#define monitor_refresh_rate 60
#define game_update_hz monitor_refresh_rate
#define target_ms_per_frame (u32)(1000.0f / (f32)game_update_hz)

    // Get the current value of performance counter.
    // This can be used to find number of 'counts' per frame. Then, by dividing
    // with perf_counter_frequency, we can find how long it took (in seconds) to
    // render this frame.
    u64 last_counter_value = win32_get_perf_counter_value();

    u64 last_timestamp_value = __rdtsc();

    u32 play_cursor_positions[game_update_hz] = {0};
    u32 play_cursor_position_index = 0;

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

        // Get controller input (via xinput).
        for (u32 controller_index = 0; controller_index < XUSER_MAX_COUNT;
             controller_index++)
        {
            XINPUT_STATE controller_state = {0};
            if (xinput_get_state(controller_index, &controller_state) ==
                ERROR_SUCCESS)
            {
                // Controller is connected.
                XINPUT_GAMEPAD *game_pad = &controller_state.Gamepad;
                ASSERT(game_pad);

                b32 dpad_up = game_pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                b32 dpad_down = game_pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                b32 dpad_left = game_pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                b32 start = game_pad->wButtons & XINPUT_GAMEPAD_START;
                b32 back = game_pad->wButtons & XINPUT_GAMEPAD_BACK;

                b32 button_a = game_pad->wButtons & XINPUT_GAMEPAD_A;
                b32 button_b = game_pad->wButtons & XINPUT_GAMEPAD_B;
                b32 button_x = game_pad->wButtons & XINPUT_GAMEPAD_X;
                b32 button_y = game_pad->wButtons & XINPUT_GAMEPAD_Y;
            }
            else
            {
                // Controller is disconnected.
            }
        }

        const win32_window_dimensions_t window_dimensions =
            win32_get_window_client_dimensions(window);

        // Fill secondary buffer (for audio).

        DWORD current_play_cursor = 0;
        DWORD current_write_cursor = 0;
        DWORD bytes_to_write = 0;
        u32 write_lock_offset = 0;

        b32 should_sound_play = false;

        if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(
                g_secondary_buffer, &current_play_cursor,
                &current_write_cursor)))
        {

            u32 target_lock_offset = (current_play_cursor +
                                      (sound_output.samples_to_write_per_frame *
                                       sound_output.bytes_per_sample)) %
                                     sound_output.secondary_buffer_size;

            write_lock_offset = (sound_output.running_sample_index *
                                 sound_output.bytes_per_sample) %
                                sound_output.secondary_buffer_size;

            if (write_lock_offset > target_lock_offset)
            {
                bytes_to_write = sound_output.secondary_buffer_size -
                                 write_lock_offset;   // for region 0.
                bytes_to_write += target_lock_offset; // for region 1.
            }
            else
            {
                bytes_to_write =
                    target_lock_offset - write_lock_offset; // for region 0.
            }
            should_sound_play = true;
        }

        // Render and update the game.
        game_offscreen_buffer_t game_offscreen_buffer = {0};
        game_offscreen_buffer.framebuffer_memory =
            g_backbuffer.framebuffer_memory;
        game_offscreen_buffer.width = g_backbuffer.width;
        game_offscreen_buffer.height = g_backbuffer.height;

        game_sound_buffer_t game_sound_buffer = {0};
        game_sound_buffer.buffer = game_sound_buffer_memory;
        game_sound_buffer.samples_to_output =
            bytes_to_write / sound_output.bytes_per_sample;
        game_sound_buffer.samples_per_second = sound_output.samples_per_second;

        game_input_t game_input = {0};
        game_input.keyboard_state = current_game_input_ptr->keyboard_state;
        game_input.delta_time = delta_time;

        game_input_t *temp = current_game_input_ptr;
        current_game_input_ptr = prev_game_input_ptr;
        prev_game_input_ptr = temp;

        game_update_and_render(&game_offscreen_buffer, &game_sound_buffer,
                               &game_input, &game_memory);

        if (should_sound_play)

        {
            win32_fill_sound_buffer(&sound_output, current_play_cursor,
                                    bytes_to_write, game_sound_buffer_memory);
        }
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

        // TODO: remove this, as it is only present for the linter to detect
        // that PRISM_DEBUG is #defined currently.
#define PRISM_DEBUG 1
#ifdef PRISM_DEBUG
        // Debug the play cursor position to the backbuffer for debugging
        // purposes.
        play_cursor_positions[play_cursor_position_index++] =
            current_play_cursor;

        if (play_cursor_position_index >= ARRAY_COUNT(play_cursor_positions))
        {

            play_cursor_position_index = 0;
            play_cursor_position_index = 0;
        }

        win32_debug_play_cursor_position(
            play_cursor_positions, ARRAY_COUNT(play_cursor_positions),
            &g_backbuffer, &sound_output, frame_index);
#endif

        win32_render_buffer_to_window(&g_backbuffer, device_context,
                                      window_dimensions.width,
                                      window_dimensions.height);

        end_counter_value = win32_get_perf_counter_value();

        ms_for_frame = win32_get_time_delta_ms(
            last_counter_value, end_counter_value, perf_counter_frequency);

        delta_time = ms_for_frame;

        u64 end_timestamp_value = __rdtsc();
        u64 clock_cycles_per_frame = end_timestamp_value - last_timestamp_value;

        // 1 frames time -> milli_seconds_for_frame = 1 / seconds in
        // dimension. To find the frames per second, we do -> 1 / time for
        // single frame.
        i32 fps = truncate_f32_to_i32(1000 / delta_time);

        char text[256];
        sprintf(text,
                "MS for frame : %f ms, FPS : %d, Clocks per frame :  %llu\n ",
                ms_for_frame, fps, clock_cycles_per_frame);
        OutputDebugStringA(text);

        last_counter_value = end_counter_value;
        last_timestamp_value = end_timestamp_value;

        frame_index++;
    }

    return 0;
}
