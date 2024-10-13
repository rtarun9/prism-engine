#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>

#include <dsound.h>
#include <xinput.h>

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

#define true 1
#define false 0

// #defines to make usage of 'static' based on purpose more clear.
#define internal static
#define global_variable static
#define local_persist static

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
win32_get_window_dimensions(const HWND window)
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
    StretchDIBits(device_context, 0, 0, window_width, window_height, 0, 0,
                  buffer->width, buffer->height, buffer->framebuffer_memory,
                  &buffer->bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

internal void win32_render_gradient_to_framebuffer(
    win32_offscreen_buffer_t *const buffer, const u32 x_shift,
    const u32 y_shift)
{
    u32 *row = buffer->framebuffer_memory;
    u32 pitch = buffer->width;

    for (u32 y = 0; y < buffer->height; y++)
    {
        u32 *pixel = row;
        for (u32 x = 0; x < buffer->width; x++)
        {
            u8 red = ((x + x_shift) & 0xff);
            u8 blue = ((y + y_shift) & 0xff);
            u8 green = 0;
            u8 alpha = 0xff;

            // Layout in memory is : BB GG RR XX
            *pixel++ = blue | (green << 8) | (red << 16) | (alpha << 24);
        }

        row += pitch;
    }
}

internal void win32_resize_framebuffer(win32_offscreen_buffer_t *const buffer,
                                       const u32 width, const u32 height)
{
    if (buffer->framebuffer_memory)
    {
        VirtualFree(buffer->framebuffer_memory, 0, MEM_RELEASE);
    }

    buffer->framebuffer_memory =
        VirtualAlloc(0, sizeof(u32) * width * height, MEM_COMMIT | MEM_RESERVE,
                     PAGE_READWRITE);

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

internal LRESULT CALLBACK win32_window_proc(HWND window, UINT message,
                                            WPARAM w_param, LPARAM l_param)
{
    LRESULT result = {0};

    switch (message)
    {

    case WM_PAINT: {
        PAINTSTRUCT paint = {0};
        HDC device_context = BeginPaint(window, &paint);

        win32_window_dimensions_t window_dimensions =
            win32_get_window_dimensions(window);

        // NOTE: In WM_PAINT, the backbuffer is NOT being rendered to, but the
        // window is being rendered alone. Check if this is suitable.
        win32_render_buffer_to_window(&g_backbuffer, device_context,
                                      window_dimensions.width,
                                      window_dimensions.height);

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
    HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS,             \
                        LPUNKNOWN pUnkOuter)
typedef DEF_DSOUND_CREATE(dsound_create_t);

DEF_DSOUND_CREATE(dsound_create_stub)
{
    return 0;
}

internal dsound_create_t *dsound_create = dsound_create_stub;

internal void win32_init_dsound(HWND window, u32 secondary_buffer_size,
                                u32 samples_per_second)
{
    HMODULE dsound_dll = LoadLibraryW(L"dsound.dll");
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
                primary_buffer_desc.dwBufferBytes = 0;
                primary_buffer_desc.lpwfxFormat = NULL;
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
                        secondary_buffer_desc.guid3DAlgorithm = DS3DALG_DEFAULT;

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
    HWND window = CreateWindowExW(
        0, window_class.lpszClassName, L"prism-engine", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1080, NULL, NULL, instance, NULL);
    ShowWindow(window, SW_SHOWNORMAL);

    if (!window)
    {
        // TODO: Logging.
        return -1;
    }

    // One byte for left channel, one byte for right channel.
    const u32 bytes_per_sample = sizeof(u16) * 2;
    const u32 samples_per_second = 48000;
    const u32 secondary_buffer_size = bytes_per_sample * samples_per_second * 2;

    win32_init_dsound(window, secondary_buffer_size, samples_per_second);
    IDirectSoundBuffer_Play(g_secondary_buffer, 0, 0, DSBPLAY_LOOPING);

    HDC device_context = GetDC(window);

    win32_resize_framebuffer(&g_backbuffer, 1080, 720);

    u32 x_shift = 0;
    u32 y_shift = 0;

    // Explanation of square wave.
    // The frequency / hertz used in that of middle C, i.e 261.63 (I will be
    // taking 256 because of familiarity).
    // This means middle c oscilates from one peak to another 256 times a
    // second.
    u32 middle_c_frequency = 256;
    // The wavelength will determine how many samples it takes to cover the
    // above frequency.
    u32 middle_c_period = samples_per_second / middle_c_frequency;

    // Now, half of that value will be the amount of samples a 'up' wave is
    // produced, and vice versa.
    u32 half_middle_c_period = middle_c_period / 2;
    u32 running_sample_index = 0;

    b32 quit = false;
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
            b32 key_down = keyboard_state[VK_RIGHT] & 0x80;
            b32 key_left = keyboard_state[VK_LEFT] & 0x80;
            b32 key_right = keyboard_state[VK_RIGHT] & 0x80;
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

                b32 dpad_up = game_pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                b32 dpad_down = game_pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                b32 dpad_left = game_pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                b32 dpad_right = game_pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

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

        RECT client_rect = {0};

        win32_window_dimensions_t window_dimensions =
            win32_get_window_dimensions(window);

        // Render gradient to the framebuffer.
        win32_render_gradient_to_framebuffer(&g_backbuffer, x_shift, y_shift);

        // Fill secondary buffer (for audio).
        DWORD current_play_cursor = 0;
        DWORD current_write_cursor = 0;

        if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(
                g_secondary_buffer, &current_play_cursor,
                &current_write_cursor)))
        {
            u32 write_lock_offset = (running_sample_index * bytes_per_sample) %
                                    secondary_buffer_size;

            DWORD bytes_to_write = 0;
            if (write_lock_offset > current_play_cursor)
            {
                // bytes_to_write =
                // secondary_buffer_size - write_lock_offset; // for region 0.
                // bytes_to_write += current_play_cursor;         // for
                // region 1.
            }
            else
            {
                bytes_to_write =
                    current_play_cursor - write_lock_offset; // for region 0.
            }

            void *region_0 = 0;
            DWORD region_0_bytes = 0;

            void *region_1 = 0;
            DWORD region_1_bytes = 0;

            if (SUCCEEDED(IDirectSoundBuffer_Lock(
                    g_secondary_buffer, write_lock_offset, bytes_to_write,
                    &region_0, &region_0_bytes, &region_1, &region_1_bytes, 0)))
            {
                i16 *sample_region_0 = (i16 *)region_0;

                DWORD region_0_sample_count = region_0_bytes / bytes_per_sample;
                for (DWORD sample_count = 0;
                     sample_count < region_0_sample_count; sample_count++)
                {
                    running_sample_index =
                        (running_sample_index + 1) % middle_c_period;

                    i8 sample_value = 0;
                    if (running_sample_index < half_middle_c_period)
                    {
                        sample_value = (i16)3000;
                    }
                    else
                    {
                        sample_value = (i16)-3000;
                    }

                    *sample_region_0++ = sample_value;
                    *sample_region_0++ = sample_value;
                }

                i16 *sample_region_1 = (i16 *)region_1;

                DWORD region_1_sample_count = region_1_bytes / bytes_per_sample;
                for (DWORD sample_count = 0;
                     sample_count < region_1_sample_count; sample_count++)
                {
                    running_sample_index =
                        (running_sample_index + 1) % middle_c_period;

                    i8 sample_value = 0;
                    if (running_sample_index < half_middle_c_period)
                    {
                        sample_value = (i16)3000;
                    }
                    else
                    {
                        sample_value = (i16)3000;
                    }

                    *sample_region_1++ = sample_value;
                    *sample_region_1++ = sample_value;
                }

                IDirectSoundBuffer_Unlock(g_secondary_buffer, region_0,
                                          region_0_bytes, region_1,
                                          region_1_bytes);
            }
        }

        win32_render_buffer_to_window(&g_backbuffer, device_context,
                                      window_dimensions.width,
                                      window_dimensions.height);

        x_shift++;
        y_shift += 2;
    }

    return 0;
}
