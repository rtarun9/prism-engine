#include "windows.h"

LRESULT CALLBACK window_proc(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_CLOSE: {
        DestroyWindow(window_handle);
    }
    break;

    case WM_DESTROY: {
        PostQuitMessage(0);
    }
    break;

    default: {
        return DefWindowProc(window_handle, message, wparam, lparam);
    }
    break;
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_command)
{
    // Create the window class, which defines a set of behaviours that multiple windows might have in common.
    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = window_proc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance;
    window_class.lpszClassName = "BaseWindowClass";

    if (!RegisterClassA(&window_class))
    {
        OutputDebugStringA("Failed to create window class.");
    }

    // Create a window.
    HWND window_handle = CreateWindowA("BaseWindowClass", "prism-engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                       CW_USEDEFAULT, 1080, 720, NULL, NULL, instance, NULL);
    if (window_handle == NULL)
    {
        OutputDebugStringA("Failed to create window.");
    }

    ShowWindow(window_handle, SW_SHOWNORMAL);

    // Main game loop.
    while (1)
    {
        MSG message = {};
        if (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        // WM_QUIT message is posted when PostQuitMessage is called.
        if (message.message == WM_QUIT)
        {
            break;
        }
    }

    MessageBoxA(NULL, "Terminating window", "X", MB_OK);

    return 0;
}
