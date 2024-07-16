#include "windows.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    MessageBox(0, "Hello, World!", "Test Message Box", MB_OK);

    return 0;
}