@echo off
setlocal EnableExtensions

cd /d "%~dp0"

echo.
echo ============================================================
echo   EvolveMusic Cloudflare API deploy
echo ============================================================
echo.

where node >nul 2>nul
if errorlevel 1 (
    echo ERROR: Node.js was not found.
    exit /b 1
)

call npm install
if errorlevel 1 exit /b 1

echo.
echo If Wrangler asks you to sign in, finish the Cloudflare browser login.
echo.

call npx wrangler deploy
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo DEPLOY COMPLETE
echo.
echo Copy the workers.dev URL printed above, then run:
echo   cd /d F:\EvolveMusic
echo   scripts\set_cloud_api.bat https://YOUR-WORKER.workers.dev
echo ============================================================
