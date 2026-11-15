@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "LOG_FILE=%~1"
if "%LOG_FILE%"=="" set "LOG_FILE=serial_com9.log"

if defined PYTHON_EXE (
  set "PYTHON=%PYTHON_EXE%"
) else (
  set "PYTHON=py -3"
)

%PYTHON% -I "%SCRIPT_DIR%realtime_bridge.py" --log-dir "%SCRIPT_DIR%" --file "%LOG_FILE%" --archive-dir "%SCRIPT_DIR%archive"
set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" (
  echo.
  echo Bridge exited with code %EXIT_CODE%.
  echo Install Python 3 or set PYTHON_EXE to the Python executable.
  pause
)
exit /b %EXIT_CODE%
