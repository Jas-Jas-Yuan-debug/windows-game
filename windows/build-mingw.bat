@echo off
REM Build claudegame on Windows with MinGW-w64 (MSYS2).
REM Requirements: MSYS2 with mingw-w64-x86_64 toolchain + raylib + openssl.
REM Run from the MSYS2 MINGW64 shell, or set PATH to include the MinGW64 bin dir.

setlocal
pushd %~dp0..
if not exist build-mingw mkdir build-mingw
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :err
cmake --build build-mingw -j
if errorlevel 1 goto :err
echo.
echo Built:
echo   build-mingw\claudegame.exe
echo   build-mingw\claudegame_server.exe
popd
exit /b 0
:err
popd
exit /b 1
