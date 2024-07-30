REM @echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM The first script argument can be : (a) open_debugger or (b) no arguments (for compilation only) or (c) run

REM explanation of all compiler flags used : 
REM /TC -> Specifies all source files are C.
REM /Fe -> rename the executable.
REM /MT -> Statistically link with the CRT.
REM /nologo -> Supress messages of optimizing compilers and other logo messages.
REM /WX -> Treat warnings as errors.
REM /wdX -> Ignore some errors.
REM /W4 -> Warning level 4
REM /Od -> No optimizations.
REM /Oi -> Use compiler intrinsics.
REM /Zi -> Generate complete debugging information.
REM /Z7 -> Similar to /Zi, but doesn't generate the vc140.pdb file.
REM /LD -> Create DLL.

mkdir build
pushd build
cl /TC /Fe"main.exe" /DPRISM_DEBUG=1 /MT /nologo /WX /wd4100 /wd4189 /W4 /Od /Oi /Z7  ../src/platform/win32_main.c /link /SUBSYSTEM:windows user32.lib gdi32.lib Winmm.lib

REM reference that tells how to append timestamp to file name in bat files : https://stackoverflow.com/questions/23226233/how-to-add-date-and-time-to-filename-in-dos
set pdb_timestamp=%date:~7,2%-%date:~4,2%-%date:~10,4%_%time:~0,2%_%time:~3,2%_%time:~6,2%
cl /Fd:game_%pdb_timestamp%.pdb /TC /DPRISM_DEBUG=1 /MT /nologo /WX /wd4100 /wd4189 /W4 /Od /Oi /Z7  /LD ../src/game.c 
popd

IF %1.==. GOTO end

IF "%1"=="open_debugger" (
    devenv build/main.exe
)

IF "%1"=="run" (
    cd build/
    main.exe
    cd ../
)

:end
