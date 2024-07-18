@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set open_vs_debugger=0

mkdir build
pushd build
cl /Zi ../src/main.c /link /SUBSYSTEM:windows user32.lib 
popd

IF %open_vs_debugger%==1 (
    devenv build/main.exe
)
