@echo off
REM DOSBox Staging - Windows 11 Development Environment Setup Launcher
REM This batch file provides a convenient way to run the PowerShell setup script
REM You can double-click this file from Windows Explorer

setlocal enabledelayedexpansion

REM Get the directory where this batch file is located
set SCRIPT_DIR=%~dp0

REM Check if PowerShell is available
where powershell >nul 2>nul
if errorlevel 1 (
    echo ERROR: PowerShell is not found on this system.
    echo Please ensure you have PowerShell 5.0 or higher installed.
    pause
    exit /b 1
)

REM Check for the setup script
if not exist "%SCRIPT_DIR%setup-dev-env-windows.ps1" (
    echo ERROR: setup-dev-env-windows.ps1 not found in %SCRIPT_DIR%
    echo Please ensure this batch file is in the same directory as the setup script.
    pause
    exit /b 1
)

echo.
echo ========================================
echo DOSBox Staging - Setup Launcher
echo ========================================
echo.
echo This will launch the Windows 11 development environment setup.
echo.
echo Choose an option:
echo [1] Full installation (includes optional tools)
echo [2] Core only (skip optional tools)
echo [3] View setup guide
echo [4] Exit
echo.

set /p choice="Enter your choice (1-4): "

if "%choice%"=="1" (
    echo.
    echo Launching setup with full installation...
    echo.
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%setup-dev-env-windows.ps1"
    exit /b !errorlevel!
)

if "%choice%"=="2" (
    echo.
    echo Launching setup with core tools only...
    echo.
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%setup-dev-env-windows.ps1" -SkipOptional
    exit /b !errorlevel!
)

if "%choice%"=="3" (
    echo.
    if exist "%SCRIPT_DIR%WINDOWS_DEV_SETUP.md" (
        echo Opening setup guide in default text editor...
        start notepad "%SCRIPT_DIR%WINDOWS_DEV_SETUP.md"
    ) else (
        echo Setup guide not found. Check the repository for WINDOWS_DEV_SETUP.md
        pause
    )
    exit /b 0
)

if "%choice%"=="4" (
    echo Exiting...
    exit /b 0
)

echo Invalid choice. Please run again and select 1-4.
pause
exit /b 1
