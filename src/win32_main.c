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

    // Frequency is the number of cycles completed in a second. (a real world
    // unit).
    u32 frequency;

    // Period is the duration required to complete a single wave cycle.
    u32 period_in_samples;

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

internal LRESULT CALLBACK win32_window_proc(HWND window, UINT message,
                                            WPARAM w_param, LPARAM l_param)
{
    LRESULT result = {0};

    switch (message)
    {

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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR command_line, int command_show)
{
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
    sound_output.frequency = 256;
    sound_output.period_in_samples =
        sound_output.samples_per_second / sound_output.frequency;
    sound_output.running_sample_index = 0;
    sound_output.samples_to_write_per_frame =
        sound_output.samples_per_second / 20;

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
    LARGE_INTEGER perf_frequency = {0};
    QueryPerformanceFrequency(&perf_frequency);

    // Get the current value of performance counter.
    // This can be used to find number of 'counts' per frame. Then, by dividing
    // with perf_frequency, we can find how long it took (in seconds) to render
    // this frame.
    LARGE_INTEGER last_counter_value = {0};
    QueryPerformanceCounter(&last_counter_value);

    u64 last_timestamp_value = __rdtsc();

    while (!quit)
    {
        BYTE keyboard_state[256] = {0};
        MSG message = {0};

        while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (message.message == WM_QUIT)
        {
            quit = true;
        }

        // Get keyboard input.
        if (GetKeyboardState(&keyboard_state[0]))
        {
            if (keyboard_state[VK_ESCAPE] & 0x80)
            {
                quit = true;
            }

            b32 key_w = keyboard_state['W'] & 0x80;
            b32 key_a = keyboard_state['A'] & 0x80;
            b32 key_s = keyboard_state['S'] & 0x80;
            b32 key_d = keyboard_state['D'] & 0x80;

            b32 key_up = keyboard_state[VK_UP] & 0x80;
            b32 key_down = keyboard_state[VK_DOWN] & 0x80;
            b32 key_left = keyboard_state[VK_LEFT] & 0x80;
            b32 key_right = keyboard_state[VK_RIGHT] & 0x80;

            if (key_up)
            {
                sound_output.frequency++;
                sound_output.period_in_samples =
                    sound_output.samples_per_second / sound_output.frequency;
            }
            else if (key_down)
            {
                sound_output.frequency--;
                sound_output.period_in_samples =
                    sound_output.samples_per_second / sound_output.frequency;
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
        game_sound_buffer.period_in_samples = sound_output.period_in_samples;
        game_sound_buffer.samples_to_output =
            bytes_to_write / sound_output.bytes_per_sample;

        game_update_and_render(&game_offscreen_buffer, &game_sound_buffer);

        if (should_sound_play)

        {
            win32_fill_sound_buffer(&sound_output, current_play_cursor,
                                    bytes_to_write, game_sound_buffer_memory);
        }

        win32_render_buffer_to_window(&g_backbuffer, device_context,
                                      window_dimensions.width,
                                      window_dimensions.height);

        LARGE_INTEGER end_counter_value = {0};
        QueryPerformanceCounter(&end_counter_value);

        i32 counts_for_frame =
            (end_counter_value.QuadPart - last_counter_value.QuadPart);
        f32 milli_seconds_for_frame =
            (1000.0f * counts_for_frame) / (f32)perf_frequency.QuadPart;

        u64 end_timestamp_value = __rdtsc();
        i32 clock_cycles_per_frame = end_timestamp_value - last_timestamp_value;

        // 1 frames time -> milli_seconds_for_frame = 1 / seconds in dimension.
        // To find the frames per second, we do -> 1 / time for single frame.
        i32 fps = 1000 / milli_seconds_for_frame;

        char text[256];
        sprintf(text, "MS for frame : %f ms, FPS : %d, Clocks per frame : %d\n",
                milli_seconds_for_frame, fps, clock_cycles_per_frame);
        OutputDebugStringA(text);

        last_counter_value = end_counter_value;
        last_timestamp_value = end_timestamp_value;
    }

    return 0;
}