@echo off
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake was not found. Install CMake and add it to PATH.
    exit /b 1
)

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build --parallel
if errorlevel 1 exit /b 1

echo.
echo Build complete: %~dp0build\piggywrite.exe
