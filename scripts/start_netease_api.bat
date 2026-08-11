@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "API=%ROOT%\runtime\netease-api"

if not exist "%API%\app.js" (
    call "%~dp0setup_netease_api.bat"
    if errorlevel 1 exit /b 1
)

if not exist "%API%\node_modules" (
    call "%~dp0setup_netease_api.bat"
    if errorlevel 1 exit /b 1
)

echo.
echo [EvolveMusic] Starting NetEase source on http://127.0.0.1:3000
pushd "%API%"
set "PORT=3000"

rem Keep source selection rights-aware: do not enable upstream music unlocking.
set "ENABLE_GENERAL_UNBLOCK=false"

node app.js
set "ERR=%errorlevel%"
popd
exit /b %ERR%
