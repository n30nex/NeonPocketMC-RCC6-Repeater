@echo off
setlocal
title NeonPocketMC RCC6 Repeater Setup

set "VENV=%LOCALAPPDATA%\NeonPocketMC-RCC6-Repeater\configurator"
set "PYTHON="
where py >nul 2>&1 && set "PYTHON=py -3"
if not defined PYTHON where python >nul 2>&1 && set "PYTHON=python"

if not defined PYTHON (
  echo Python 3 is required.
  echo Install it from https://www.python.org/downloads/windows/ and run this file again.
  pause
  exit /b 1
)

if not exist "%VENV%\Scripts\python.exe" (
  echo First run: preparing the small USB helper...
  %PYTHON% -m venv "%VENV%" || goto :failed
  "%VENV%\Scripts\python.exe" -m pip install --disable-pip-version-check pyserial==3.5 || goto :failed
)

"%VENV%\Scripts\python.exe" "%~dp0scripts\configure_rcc6.py" %*
set "RESULT=%ERRORLEVEL%"
echo.
pause
exit /b %RESULT%

:failed
echo.
echo Could not prepare the USB helper. See the manual browser setup links in README.md.
pause
exit /b 1
