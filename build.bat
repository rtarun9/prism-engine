REM @echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM The first script argument can be : (a) open_debugger or (b) empty (for compilation only) or (c) run

mkdir build
pushd build
cl /TC /Fe"main.exe" /DPRISM_DEBUG=1 /Zi ../src/platform/win32_main.c /link /SUBSYSTEM:windows user32.lib gdi32.lib 
popd

IF %1.==. GOTO end

IF "%1"=="open_debugger" (
    devenv build/main.exe
)

IF "%1"=="run" (
    cd build/
    main.exe
)

:end
