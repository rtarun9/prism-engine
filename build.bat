@echo off

mkdir build
pushd build

:: Documentation for compiler options : https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170

:: Zi : used to create .pdb that contains information useful for debugging.
:: Od : to disable optimizations (in debug mode)
:: fp:precise : Precise floating point model, with predictable results.
:: Fe : Specify name of executable.
:: D : Defines macros.
:: FC : Displays the full path of source code files passed to cl.exe in diagnostic text.
:: nologo : Supress information indicating compiler version.
:: Oi : Replace some function calls with compiler intrinsics.
:: MT : Statically link with CRT. This is done so that there is no need to redistribute the CRT version along with the game.
:: W4 : Displays quite a few useful warnings.
:: Wx : treat warning as errors

:: wd : Ignore particular compiler warnings.
REM Below are explanations for the various warnings that are ignored.
:: C4100 : Unreferenced formal parameters.
:: C4127 : Constant if statement.
:: C4996 : sprintf deprecation (NOTE: sprintf should not be used in future).
:: C4189 : Local variable initialized but not referenced.

cl.exe /nologo /DPRISM_DEBUG /Oi /Od /Zi /FC /W4 /WX /wd4100 /wd4127 /wd4996 /wd4189 /MT /fp:precise ../src/win32_main.c /Fe:game.exe user32.lib gdi32.lib Winmm.lib
game.exe
popd


