@echo off
setlocal
call "%~dp0setup_evolveui.bat" || exit /b 1
call "%~dp0setup_netease_api.bat" || exit /b 1
call "%~dp0check_webview.bat"
if errorlevel 2 (
    echo.
    echo [EvolveMusic] Continuing without embedded web login.
    echo Install Qt WebView later if you need platform web login.
)
call "%~dp0build_windows.bat" || exit /b 1
echo.
echo Everything is ready.
echo Run scripts\run_windows.bat to start the source service and EvolveMusic.
