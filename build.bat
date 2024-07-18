REM @echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM the first script argument determines if debugger must be opened. 1 opens the debugger, 0 does not.
set open_vs_debugger=%1

REM the second script argument will run the built executable if the value is 1. Will only occur if debugger is not opened.
set run_executable=%2

mkdir build
pushd build
cl /Zi ../src/main.c /link /SUBSYSTEM:windows user32.lib gdi32.lib
popd

IF %open_vs_debugger%==1 (
    devenv build/main.exe
)

IF %run_executable%==1 (
    start "" build/main.exe
)
