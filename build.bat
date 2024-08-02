@echo on 
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM The first script argument can be : (a) open_debugger or (b) no arguments (for compilation only) (c) run

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
REM /PDB -> Give a custom name to the generated program database (PDB) file.

mkdir build
pushd build

del *.pdb
del *.raddbg
del *.sta

cl /TC /Fe"main.exe" /DPRISM_DEBUG=1 /MT /nologo /WX /wd4100 /wd4189 /W4 /Od /Oi /Z7  ../src/platform/win32_main.c /link /SUBSYSTEM:windows user32.lib gdi32.lib Winmm.lib

REM reference that tells how to append timestamp to file name in bat files : https://stackoverflow.com/questions/23226233/how-to-add-date-and-time-to-filename-in-dos
set dd=%date:~0,2%
set mm=%date:~3,2%
set yyyy=%date:~6,8%
set mm=%time:~3,2%
set ss=%time:~6,2%

set pdb_file_timestamp=game__%dd%_%mm%_%yyyy%_%mm%_%ss%
cl /TC /DPRISM_DEBUG=1 /MT /nologo /WX /wd4100 /wd4189 /W4 /Od /Oi /Z7  /LD ../src/game.c /link /pdb:%pdb_file_timestamp%.pdb  
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
