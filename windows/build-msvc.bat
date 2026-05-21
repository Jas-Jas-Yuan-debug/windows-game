@echo off
REM Build claudegame on Windows with MSVC + vcpkg.
REM Requirements: Visual Studio 2022 (Desktop C++), CMake, and vcpkg installed.
REM Set VCPKG_ROOT to your vcpkg checkout before running, e.g.
REM   set VCPKG_ROOT=C:\vcpkg

setlocal
if "%VCPKG_ROOT%"=="" (
    echo VCPKG_ROOT is not set. Install vcpkg from https://github.com/microsoft/vcpkg and set VCPKG_ROOT.
    exit /b 1
)
pushd %~dp0..
if not exist build-msvc (
    cmake -S . -B build-msvc -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -G "Visual Studio 17 2022" -A x64
    if errorlevel 1 goto :err
)
cmake --build build-msvc --config Release -j
if errorlevel 1 goto :err
echo.
echo Built:
echo   build-msvc\Release\claudegame.exe
echo   build-msvc\Release\claudegame_server.exe
popd
exit /b 0
:err
popd
exit /b 1
