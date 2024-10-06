@echo off

REM vcvarsall.bat needs to be called for cl.exe and other dev cli tools to be used.
REM To run this script on shell startup, add /k <path to this bat file> under command prompt's command line.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
