#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param,
                             LPARAM l_param)
{
    LRESULT result = {0};

    switch (message)
    {
    case WM_SIZE: {
        OutputDebugStringW(L"WM_SIZE");
    }
    break;

    case WM_PAINT: {
        PAINTSTRUCT paint = {0};
        HDC device_context = BeginPaint(window, &paint);

        const HBRUSH gray_brush = CreateSolidBrush(0x00555555);

        FillRect(device_context, &paint.rcPaint, gray_brush);

        EndPaint(window, &paint);

        OutputDebugStringW(L"WM_PAINT");
    }
    break;

    case WM_CLOSE: {
        DestroyWindow(window);
    }
    break;

    case WM_DESTROY: {
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR command_line, int command_show)
{
    // Register a window class : set of behaviors that several windows might
    // have in common (required even if only a single window is created by the
    // application).
    WNDCLASSEXW window_class = {0};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.style = CS_VREDRAW | CS_HREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"CustomWindowClass";

    RegisterClassExW(&window_class);

    // Create the actual window.

    HWND window = CreateWindowExW(0, window_class.lpszClassName,
                                  L"prism-engine", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, NULL, NULL, instance, NULL);
    ShowWindow(window, SW_SHOWNORMAL);

    if (!window)
    {
        // TODO: Logging.
        return -1;
    }

    BYTE keyboard_state[256] = {0};

    BOOL quit = FALSE;
    while (!quit)
    {
        MSG message = {0};
        while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (message.message == WM_QUIT)
        {
            quit = TRUE;
        }

        if (GetKeyboardState(&keyboard_state[0]))
        {
            if (keyboard_state[VK_ESCAPE] & 0x80)
            {
                quit = TRUE;
            }
        }
    }

    return 0;
}
