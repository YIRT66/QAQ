@echo off
setlocal EnableExtensions
set "QT_ROOT=E:\QT\6.10.0\mingw_64"
set "WV2_GUID={F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"

echo.
echo ============================================================
echo  EvolveMusic WebView / WebView2 Runtime Diagnostic
echo ============================================================
echo.

set "FAIL=0"

if exist "%QT_ROOT%\lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake" (
  echo [OK] Qt6WebView CMake package
) else (
  echo [FAIL] Qt6WebView CMake package missing
  set "FAIL=1"
)

if exist "%QT_ROOT%\qml\QtWebView\qmldir" (
  echo [OK] QtWebView QML module
) else (
  echo [FAIL] QtWebView QML module missing
  set "FAIL=1"
)

echo.
echo WebView2 Runtime:
set "WV2_FOUND="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\%WV2_GUID%" /v pv 2^>nul ^| findstr /i "pv"') do (
  echo [OK] Machine WebView2 Runtime: %%B
  set "WV2_FOUND=1"
)
for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Microsoft\EdgeUpdate\Clients\%WV2_GUID%" /v pv 2^>nul ^| findstr /i "pv"') do (
  echo [OK] User WebView2 Runtime: %%B
  set "WV2_FOUND=1"
)
if not defined WV2_FOUND (
  echo [FAIL] Microsoft Edge WebView2 Runtime registry entry was not found.
  set "FAIL=1"
)

echo.
if "%FAIL%"=="0" (
  echo RESULT: Qt WebView and WebView2 Runtime look available.
) else (
  echo RESULT: One or more browser runtime components are missing.
)
echo.
pause
exit /b %FAIL%
