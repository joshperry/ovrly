@echo off

set CEF_ROOT=h:\dev\cef_binary_80.1.15+g7b802c9+chromium-80.0.3987.163_windows64

set BASE_DIR=%~dp0
rem echo %BASE_DIR%

mkdir "%BASE_DIR%\build"

cd "%BASE_DIR%\build"

rem use "Visual Studio 15" as the generator for x86 builds
rem cmake -G "Visual Studio 15" "%BASE_DIR%"
cmake -G "Visual Studio 16 2019" -A x64 "%BASE_DIR%"

cd %BASE_DIR%
