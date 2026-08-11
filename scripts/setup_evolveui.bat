@echo off
setlocal EnableExtensions
set ROOT=%~dp0..
set DEST=%ROOT%\third_party\EvolveUI

echo [EvolveMusic] Preparing EvolveUI...
where git >nul 2>nul || (echo ERROR: Git is required. Install Git for Windows first. & exit /b 1)

if exist "%DEST%\components\ETheme.qml" (
  echo EvolveUI already exists. Updating...
  git -C "%DEST%" pull --ff-only
  exit /b %errorlevel%
)

if exist "%DEST%" rmdir /s /q "%DEST%"
if not exist "%ROOT%\third_party" mkdir "%ROOT%\third_party"
git clone --depth 1 https://github.com/sudoevolve/EvolveUI.git "%DEST%"
if errorlevel 1 exit /b 1

echo EvolveUI ready.
