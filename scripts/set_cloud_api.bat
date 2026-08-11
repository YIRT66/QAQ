@echo off
setlocal EnableExtensions

set "PRIMARY=%~1"
set "BACKUP=%~2"
if "%PRIMARY%"=="" set "PRIMARY=https://evolvemusic-cloud.18048369193.workers.dev"
if "%BACKUP%"=="" set "BACKUP=https://xn--fiqq40n.tyxowo.top"

echo [EvolveMusic] Saving dual-cloud endpoints for the current Windows user...
setx EVOLVE_MUSIC_CLOUD_API "%PRIMARY%" >nul
if errorlevel 1 exit /b 1
setx EVOLVE_MUSIC_CLOUD_API_BACKUP "%BACKUP%" >nul
if errorlevel 1 exit /b 1

echo.
echo [OK] Primary: %PRIMARY%
echo [OK] Backup:  %BACKUP%
echo Restart EvolveMusic for the new endpoints to take effect.
exit /b 0
