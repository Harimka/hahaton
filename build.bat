@echo off
setlocal
echo ========================================================
echo  Team-04 Firmware Build (C++17, 170MHz, FPU, -O3)
echo ========================================================

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%firmware"

if not exist build (
    mkdir build
)

cmake -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=./arm-none-eabi-toolchain.cmake -DCMAKE_MAKE_PROGRAM=C:/Users/zeets/scoop/shims/ninja.exe
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

cmake --build build
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Firmware build complete!
echo Output files:
echo  - firmware\build\team04_firmware.hex
echo  - firmware\build\team04_firmware.bin
echo  - firmware\build\team04_firmware.elf
exit /b 0
