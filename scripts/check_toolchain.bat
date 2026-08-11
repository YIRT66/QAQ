@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "TEMP=%ROOT%\.tmp"
set "TMP=%ROOT%\.tmp"
if not exist "%TEMP%" mkdir "%TEMP%"

call "%~dp0qt_mingw_env.bat"
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo EvolveMusic v0.3 toolchain check
echo ============================================================
echo Qt root:
echo   %QT_ROOT%
echo.
echo qmake:
"%QT_ROOT%\bin\qmake.exe" --version
echo.
echo C++ compiler:
"%GXX%" --version
echo.
echo Make:
"%MAKE_EXE%" --version
echo.
echo CMake:
cmake --version
echo.

set "WEBVIEW_CFG=%QT_ROOT%\lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake"
if exist "%WEBVIEW_CFG%" (
    echo Embedded browser: ENABLED - Qt WebView found
    echo Windows backend: Edge WebView2
) else (
    echo Embedded browser: DISABLED - Qt WebView module not installed
    echo Run scripts\check_webview.bat for details.
)

echo.
echo Paths:
echo   Compiler = %GXX%
echo   Make     = %MAKE_EXE%
echo   Temp     = %TEMP%
echo ============================================================
pause
