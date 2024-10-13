@echo off

mkdir build
pushd build
:: Zi is used to create .pdb that contains information useful for debugging.
cl.exe /Zi ../src/main.c user32.lib gdi32.lib
main.exe
popd


