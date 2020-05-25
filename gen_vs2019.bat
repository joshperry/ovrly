@echo off

rem set CEF_ROOT environment variable
rem add it to your user environment if you want to take advantage of VS cmake integration
rem CEF_ROOT=h:\dev\cef_binary_80.1.15+g7b802c9+chromium-80.0.3987.163_windows64

set BASE_DIR=%~dp0
rem echo %BASE_DIR%

IF "%1"=="" ( SET "BUILD_DIR=build" ) ELSE ( SET "BUILD_DIR=%1" )

mkdir "%BASE_DIR%\%BUILD_DIR%"

cd "%BASE_DIR%\%BUILD_DIR%"

rem use Visual Studio as the generator
rem cmake -G "Visual Studio 15" "%BASE_DIR%"
cmake -G "Visual Studio 16 2019" -A x64 "%BASE_DIR%"

cd %BASE_DIR%
