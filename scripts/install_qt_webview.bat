@echo off
setlocal EnableExtensions
title EvolveMusic - Install Qt WebView 6.10.0 MinGW

echo.
echo ============================================================
echo  EvolveMusic - Qt WebView One-Click Installer
echo ============================================================
echo  Qt:      E:\QT\6.10.0\mingw_64
echo  Project: F:\EvolveMusic
echo  Source:  Official Qt online repository
echo ============================================================
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_qt_webview.ps1" ^
  -QtRoot "E:\QT\6.10.0\mingw_64" ^
  -ProjectRoot "F:\EvolveMusic"

set "ERR=%errorlevel%"
echo.
if not "%ERR%"=="0" (
    echo Installation failed with exit code %ERR%.
    echo Copy the entire window output back to ChatGPT if you need help.
) else (
    echo Installation completed successfully.
)
echo.
pause
exit /b %ERR%
