@echo off
setlocal enabledelayedexpansion
echo ========================================================
echo  Team-04 Firmware Flash (ST-LINK V3 / NUCLEO-G474RE)
echo ========================================================

set SCRIPT_DIR=%~dp0
set HEX_FILE=%SCRIPT_DIR%firmware\build\team04_firmware.hex
set BIN_FILE=%SCRIPT_DIR%firmware\build\team04_firmware.bin

if not exist "%HEX_FILE%" (
    echo [INFO] Firmware not found. Triggering build first...
    call "%SCRIPT_DIR%build.bat"
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] Build step failed. Flashing aborted.
        exit /b 1
    )
)

set CLI_PATH=C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\STM32_Programmer_CLI.exe

if exist "%CLI_PATH%" (
    echo Flashing via STM32_Programmer_CLI...
    "%CLI_PATH%" -c port=SWD mode=UR -w "%HEX_FILE%" -v -rst
    if !ERRORLEVEL! EQU 0 (
        echo.
        echo [SUCCESS] Board flashed and reset successfully!
        exit /b 0
    ) else (
        echo [WARNING] STM32_Programmer_CLI reported an error.
    )
)

echo Trying alternative flasher: st-flash...
where st-flash >nul 2>nul
if !ERRORLEVEL! EQU 0 (
    st-flash --reset write "%BIN_FILE%" 0x08000000
    if !ERRORLEVEL! EQU 0 (
        echo.
        echo [SUCCESS] Board flashed via st-flash!
        exit /b 0
    )
)

echo.
echo [ERROR] Flashing failed!
echo Please check:
echo 1. Nucleo-G474RE is connected via USB cable to PC (top ST-Link USB port).
echo 2. Power LED on the Nucleo board is ON.
echo 3. ST-Link drivers are loaded in Device Manager.
exit /b 1
