@echo off

mkdir build
pushd build
:: Zi is used to create .pdb that contains information useful for debugging.
cl.exe /Zi /fp:fast ../src/win32_main.c user32.lib gdi32.lib
win32_main.exe
popd


