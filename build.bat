@echo on 
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM The first script argument can be : (a) open_debugger or (b) no arguments (for compilation only) (c) run (d) or clean

REM explanation of some compiler flags used : 
REM /TC -> Specifies all source files are C.
REM /Fe -> rename the executable.
REM /MTd -> Statically link with the debug version of CRT.
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

cl /TC /Fe"main.exe" /DPRISM_DEBUG=1 /MTd /nologo /WX /wd4100 /wd4189 /W4 /Od /Oi /Z7  ../src/platform/win32_main.c /link /SUBSYSTEM:windows user32.lib gdi32.lib Winmm.lib Advapi32.lib

set pdb_file_timestamp=game_%random%_%random%_%random%
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

IF "%1"=="clean" (
    cd build/
    del /Q *
    cd ../
)

:end
