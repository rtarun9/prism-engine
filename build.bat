@echo off

mkdir build
pushd build

:: Script parameters:
:: No argument -> build game dll and platform exe.
:: game -> build the game dll

:: Documentation for compiler options : https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170

:: Zi : used to create .pdb that contains information useful for debugging.
:: Od : to disable optimizations (in debug mode)
:: fp:precise : Precise floating point model, with predictable results.
:: Fe : Specify name of executable.
:: D : Defines macros.
:: FC : Displays the full path of source code files passed to cl.exe in diagnostic text.

set common_macros=/DPRISM_DEBUG

set win32_compiler_flags=%common_macros%
set win32_compiler_flags=%win32_compiler_flags% /Z7
set win32_compiler_flags=%win32_compiler_flags% /Od
set win32_compiler_flags=%win32_compiler_flags% /fp:precise
set win32_compiler_flags=%win32_compiler_flags% /FC

:: nologo : Supress information indicating compiler version.
:: Oi : Replace some function calls with compiler intrinsics.
:: MT : Statically link with CRT. This is done so that there is no need to redistribute the CRT version along with the game.
:: W4 : Displays quite a few useful warnings.
:: Wx : treat warning as errors

set win32_compiler_flags=%win32_compiler_flags% /nologo
set win32_compiler_flags=%win32_compiler_flags% /Oi
set win32_compiler_flags=%win32_compiler_flags% /MT
set win32_compiler_flags=%win32_compiler_flags% /W4
set win32_compiler_flags=%win32_compiler_flags% /WX

:: wd : Ignore particular compiler warnings.
REM Below are explanations for the various warnings that are ignored.
:: C4100 : Unreferenced formal parameters.
:: C4127 : Constant if statement.
:: C4996 : sprintf deprecation (NOTE: sprintf should not be used in future).
:: C4189 : Local variable initialized but not referenced.

:: std : set version of C to use.

set win32_compiler_flags=%win32_compiler_flags% /wd4100
set win32_compiler_flags=%win32_compiler_flags% /wd4127
set win32_compiler_flags=%win32_compiler_flags% /wd4996
set win32_compiler_flags=%win32_compiler_flags% /wd4189
set win32_compiler_flags=%win32_compiler_flags% /WX
set win32_compiler_flags=%win32_compiler_flags% /std:c17

set win32_linker_flags=user32.lib
set win32_linker_flags=%win32_linker_flags% gdi32.lib
set win32_linker_flags=%win32_linker_flags% Winmm.lib

:: LD : Create a DLL.
:: PDB : Creates a PDB using user specified name.
:: DEBUG : Tells the linker to create a PDB (program database) that holds debugging information.

:: TODO: How should the pdb and raddgb files be deleted.
::del *.pdb
::del *.raddbg

set game_linker_flags=/DEBUG:FULL
set game_linker_flags=%game_linker_flags% /PDB:game_pdb_%RANDOM%.pdb

IF "%1"=="game" (
	cl.exe %win32_compiler_flags% ../src/game.c /LD /link %game_linker_flags%
) else IF "%1"=="" (
	cl.exe %win32_compiler_flags% ../src/game.c /LD /link %game_linker_flags%
	cl.exe %win32_compiler_flags% ../src/win32_main.c /Fe:win32_main.exe /link %win32_linker_flags%

	win32_main.exe
)

popd
