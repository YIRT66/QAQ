@echo off
setlocal EnableExtensions
title EvolveMusic - Install WebView and Rebuild

call "%~dp0install_qt_webview.bat"
if errorlevel 1 exit /b 1

if not exist "F:\EvolveMusic\scripts\build_windows.bat" (
    echo ERROR: F:\EvolveMusic\scripts\build_windows.bat was not found.
    pause
    exit /b 1
)

echo.
echo [EvolveMusic] Cleaning old CMake cache and rebuilding with Qt WebView...
cd /d F:\EvolveMusic
call scripts\build_windows.bat clean
if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo [EvolveMusic] Build completed.
pause
