@echo off
REM ========================================
REM Audio DSP Configuration - EXE Build Script
REM ========================================
echo ========================================
echo Audio DSP Configuration - EXE Build
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python is not installed or not in PATH.
    echo Please install Python 3.8+ from https://www.python.org/downloads/
    pause
    exit /b 1
)

REM Install/upgrade dependencies
echo Installing dependencies...
pip install --upgrade -r requirements.txt
pip install pyinstaller

REM Clean previous builds
echo.
echo Cleaning previous builds...
if exist "build" rmdir /s /q build
if exist "dist" rmdir /s /q dist

REM Build EXE
echo.
echo Building EXE...
pyinstaller --clean build_exe.spec

REM Check if build succeeded
if exist "dist\AudioDSPConfig.exe" (
    echo.
    echo ========================================
    echo BUILD SUCCESSFUL!
    echo ========================================
    echo EXE location: dist\AudioDSPConfig.exe
    echo.
    dir dist\AudioDSPConfig.exe
) else (
    echo.
    echo ========================================
    echo BUILD FAILED!
    echo ========================================
    echo Check the output above for error details.
    echo.
    exit /b 1
)

pause
