@echo off
setlocal
set "QT=E:\QT\6.10.0\mingw_64"

echo Qt WebView verification
echo =======================
echo.

if not exist "%QT%\bin\qmake.exe" (
    echo [FAIL] Qt not found: %QT%
    exit /b 1
)

"%QT%\bin\qmake.exe" --version
echo.

if exist "%QT%\lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake" (
    echo [OK] CMake package
) else (
    echo [FAIL] CMake package missing
)

if exist "%QT%\qml\QtWebView\qmldir" (
    echo [OK] QML module
) else (
    echo [FAIL] QML module missing
)

dir /b "%QT%\bin\Qt6WebView*.dll" 2>nul
echo.

if exist "F:\EvolveMusic\scripts\check_webview.bat" (
    echo EvolveMusic project check:
    call "F:\EvolveMusic\scripts\check_webview.bat"
)

pause
