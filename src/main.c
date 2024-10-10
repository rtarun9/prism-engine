#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>

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

typedef struct
{
    u32 width;
    u32 height;
} win32_window_dimensions_t;

win32_window_dimensions_t win32_get_window_dimensions(const HWND window)
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

DWORD WINAPI XInputGetState_(DWORD dw_user_index, XINPUT_STATE *state);
DWORD WINAPI XInputSetState_(DWORD dw_user_index, XINPUT_VIBRATION *vibration);

#define def_xinput_get_state(name)                                             \
    DWORD WINAPI name(DWORD dw_user_index, XINPUT_STATE *state)
#define def_xinput_set_state(name)                                             \
    DWORD WINAPI name(DWORD dw_user_index, XINPUT_VIBRATION *vibration)

def_xinput_get_state(xinput_get_state_stub)
{
    return 0;
}

def_xinput_set_state(xinput_set_state_stub)
{
    return 0;
}

typedef def_xinput_get_state(xinput_get_state_t);
typedef def_xinput_set_state(xinput_set_state_t);

xinput_get_state_t *xinput_get_state = xinput_get_state_stub;
xinput_set_state_t *xinput_set_state = xinput_set_state_stub;

void load_xinput_functions()
{
    HMODULE xinput_dll = LoadLibraryW(L"xinput1_3.dll");

    if (xinput_dll)
    {
        xinput_get_state =
            (xinput_get_state_t *)GetProcAddress(xinput_dll, "XInputGetState");
        xinput_set_state =
            (xinput_set_state_t *)GetProcAddress(xinput_dll, "XInputSetState");
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR command_line, int command_show)
{
    load_xinput_functions();

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

    HDC device_context = GetDC(window);

    win32_resize_framebuffer(&g_backbuffer, 1080, 720);

    u32 x_shift = 0;
    u32 y_shift = 0;

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

        win32_render_gradient_to_framebuffer(&g_backbuffer, x_shift, y_shift);
        win32_render_buffer_to_window(&g_backbuffer, device_context,
                                      window_dimensions.width,
                                      window_dimensions.height);

        x_shift++;
        y_shift += 2;
    }

    return 0;
}
