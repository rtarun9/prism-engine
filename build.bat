@echo off

mkdir build
pushd build
cl.exe ../src/main.c user32.lib
main.exe
popd


