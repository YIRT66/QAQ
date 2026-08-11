@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "LOGDIR=%LOCALAPPDATA%\EvolveMusic\EvolveMusic\logs"
call "%~dp0qt_mingw_env.bat"
if errorlevel 1 exit /b 1
if not exist "%ROOT%\build-mingw\EvolveMusic.exe" (
  echo ERROR: build-mingw\EvolveMusic.exe not found.
  exit /b 1
)
start "EvolveMusic" "%ROOT%\build-mingw\EvolveMusic.exe"
timeout /t 1 /nobreak >nul
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$d='%LOGDIR%'; New-Item -ItemType Directory -Force -Path $d ^| Out-Null; $f=Join-Path $d ('evolvemusic-'+(Get-Date -Format 'yyyy-MM-dd')+'.log'); if(!(Test-Path $f)){New-Item -ItemType File -Path $f ^| Out-Null}; Write-Host ('[EvolveMusic] Tailing '+$f); Get-Content -Path $f -Wait -Tail 120"
