@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "EXE=%ROOT%\build-mingw\EvolveMusic.exe"
set "TEMP=%ROOT%\.tmp"
set "TMP=%ROOT%\.tmp"
if not defined EVOLVE_MUSIC_CLOUD_API set "EVOLVE_MUSIC_CLOUD_API=https://evolvemusic-cloud.18048369193.workers.dev"
if not defined EVOLVE_MUSIC_CLOUD_API_BACKUP set "EVOLVE_MUSIC_CLOUD_API_BACKUP=https://xn--fiqq40n.tyxowo.top"
if not exist "%TEMP%" mkdir "%TEMP%"

call "%~dp0qt_mingw_env.bat"
if errorlevel 1 exit /b 1

if not exist "%EXE%" (
    echo [EvolveMusic] EvolveMusic.exe was not found. Building first...
    call "%~dp0build_windows.bat"
    if errorlevel 1 exit /b 1
)

echo [EvolveMusic] v0.18.1 Ourcraft Music Backend + Evolve Social Cloud
echo [EvolveMusic] Primary: %EVOLVE_MUSIC_CLOUD_API%
echo [EvolveMusic] Backup:  %EVOLVE_MUSIC_CLOUD_API_BACKUP%
echo [EvolveMusic] Starting desktop client...
start "Evolve Music" "%EXE%"
exit /b 0
