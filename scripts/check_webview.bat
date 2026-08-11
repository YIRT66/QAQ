@echo off
setlocal EnableExtensions
call "%~dp0qt_mingw_env.bat"
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo EvolveMusic - embedded login browser check
echo ============================================================
echo Qt: %QT_ROOT%
echo.

set "WEBVIEW_CFG=%QT_ROOT%\lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake"
if exist "%WEBVIEW_CFG%" (
    echo [OK] Qt WebView is installed.
    echo Qt 6.10 on Windows uses the Edge WebView2 backend.
    echo Embedded platform login will be enabled at build time.
    echo.
    echo Config:
    echo   %WEBVIEW_CFG%
    pause
    exit /b 0
)

echo [MISSING] Qt WebView is not installed in this Qt kit.
echo.
echo The player itself will still build and run.
echo To enable the embedded login page, open Qt Maintenance Tool and add:
echo   Qt 6.10.0 ^> Qt WebView
echo for this existing kit:
echo   %QT_ROOT%
echo.
echo IMPORTANT:
echo   Do NOT install Qt WebEngine for this MinGW build.
echo   Qt WebView is the intended v0.3 Windows login backend.
echo.
pause
exit /b 2
