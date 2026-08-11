@echo off
setlocal EnableExtensions EnableDelayedExpansion
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\build-mingw"
set "DIST=%ROOT%\dist"
set "STAGE=%DIST%\app"
set "VERSION=0.18.1"

call "%~dp0build_windows.bat" clean
if errorlevel 1 exit /b 1
call "%~dp0qt_mingw_env.bat"
if errorlevel 1 exit /b 1

if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
copy /y "%BUILD_DIR%\EvolveMusic.exe" "%STAGE%\EvolveMusic.exe" >nul
if errorlevel 1 (
  echo ERROR: Failed to stage EvolveMusic.exe.
  exit /b 1
)

set "WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe"
if not exist "%WINDEPLOYQT%" (
  echo ERROR: windeployqt.exe was not found: %WINDEPLOYQT%
  exit /b 1
)

echo [EvolveMusic] Deploying Qt runtime...
"%WINDEPLOYQT%" --release --no-translations --qmldir "%ROOT%" "%STAGE%\EvolveMusic.exe"
if errorlevel 1 (
  echo ERROR: windeployqt failed.
  exit /b 1
)

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  echo [EvolveMusic] Inno Setup 6 was not found. Trying winget...
  where winget >nul 2>nul
  if errorlevel 1 (
    echo ERROR: Install Inno Setup 6, then run this script again.
    exit /b 1
  )
  winget install --id JRSoftware.InnoSetup -e --accept-package-agreements --accept-source-agreements
  set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
  if not exist "!ISCC!" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
)
if not exist "%ISCC%" (
  echo ERROR: ISCC.exe is still unavailable.
  exit /b 1
)

if not exist "%DIST%" mkdir "%DIST%"
echo [EvolveMusic] Building full installer...
"%ISCC%" /DMyAppVersion=%VERSION% "%ROOT%\installer\EvolveMusic.iss"
if errorlevel 1 exit /b 1

set "SETUP=%DIST%\EvolveMusic-Setup-%VERSION%.exe"
if not exist "%SETUP%" (
  echo ERROR: Installer build finished but setup EXE was not found.
  exit /b 1
)

echo.
echo ============================================================
echo INSTALLER SUCCESS
echo EXE: %SETUP%
echo.
echo SHA-256 ^(copy this value into EVOLVE_SETUP_SHA256^):
certutil -hashfile "%SETUP%" SHA256 | findstr /R /V "hash CertUtil"
echo ============================================================
echo.
echo Upload this EXE to your release host, then configure the Worker:
echo   EVOLVE_LATEST_VERSION=%VERSION%
echo   EVOLVE_SETUP_URL=https://your-download-host/EvolveMusic-Setup-%VERSION%.exe
echo   EVOLVE_SETUP_SHA256=the_hash_printed_above
exit /b 0
