#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR command_line, int command_show)
{
    MessageBox(NULL, L"My first windows program!", L"Congratulations!", MB_OK);
    return 0;
}
