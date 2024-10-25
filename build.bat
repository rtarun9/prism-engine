@echo off

mkdir build
pushd build

:: Documentation for compiler options : https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170

:: Zi is used to create .pdb that contains information useful for debugging.
:: Od is to disable optimizations (in debug mode)
:: fp:precise : Precise floating point model, with predictable results.
:: Fe : Specify name of executable.

cl.exe /Od /Zi /fp:precise ../src/win32_main.c /Fe:game.exe user32.lib gdi32.lib
game.exe
popd


