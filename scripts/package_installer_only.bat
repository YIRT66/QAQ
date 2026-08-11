@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================
rem EvolveMusic - package installer ONLY
rem Uses an existing build-mingw\EvolveMusic.exe.
rem DOES NOT run CMake and DOES NOT rebuild the program.
rem Put this file in: F:\EvolveMusic\scripts\package_installer_only.bat
rem ============================================================

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\build-mingw"
set "DIST=%ROOT%\dist"
set "STAGE=%DIST%\app"
set "QML_SCAN=%ROOT%\.deploy-qml-scan"
set "VERSION=0.18.1"
set "EXE=%BUILD_DIR%\EvolveMusic.exe"

if not exist "%EXE%" (
    echo ERROR: Existing compiled EXE was not found:
    echo   %EXE%
    echo.
    echo Build once first with:
    echo   scripts\build_windows.bat
    exit /b 1
)

call "%~dp0qt_mingw_env.bat"
if errorlevel 1 exit /b 1

set "WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe"
if not exist "%WINDEPLOYQT%" (
    echo ERROR: windeployqt.exe was not found:
    echo   %WINDEPLOYQT%
    exit /b 1
)

echo.
echo ============================================================
echo   EvolveMusic - PACKAGE ONLY
 echo   NO COMPILE / NO CMAKE REBUILD
 echo ============================================================
echo Source EXE: %EXE%
echo Stage:      %STAGE%
echo Version:    %VERSION%
echo ============================================================
echo.

rem Recreate only the deployment staging folder. Never touch build-mingw.
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%" >nul 2>nul
copy /y "%EXE%" "%STAGE%\EvolveMusic.exe" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy EvolveMusic.exe into dist\app.
    exit /b 1
)

rem Build a tiny QML scan tree so windeployqt does not recursively scan
rem build-mingw/dist/.tmp and time out in qmlimportscanner.
if exist "%QML_SCAN%" rmdir /s /q "%QML_SCAN%"
mkdir "%QML_SCAN%" >nul 2>nul

if exist "%ROOT%\Main.qml" copy /y "%ROOT%\Main.qml" "%QML_SCAN%\Main.qml" >nul
for %%D in (components pages) do (
    if exist "%ROOT%\%%D" xcopy "%ROOT%\%%D\*.qml" "%QML_SCAN%\%%D\" /E /I /Y /Q >nul
)

rem Include any third-party QML tree only if it exists.
if exist "%ROOT%\third_party" xcopy "%ROOT%\third_party\*.qml" "%QML_SCAN%\third_party\" /E /I /Y /Q >nul

echo [1/2] Deploying Qt runtime from existing EXE...
"%WINDEPLOYQT%" --release --no-translations --qmldir "%QML_SCAN%" "%STAGE%\EvolveMusic.exe"
set "DEPLOY_RC=%ERRORLEVEL%"

if exist "%QML_SCAN%" rmdir /s /q "%QML_SCAN%"

if not "%DEPLOY_RC%"=="0" (
    echo.
    echo ERROR: windeployqt failed with code %DEPLOY_RC%.
    echo The program itself was NOT rebuilt.
    echo.
    echo You can rerun this packaging script directly after fixing deployment.
    exit /b %DEPLOY_RC%
)

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo ERROR: Inno Setup 6 was not found.
    echo Install it with:
    echo   winget install --id JRSoftware.InnoSetup -e
    exit /b 1
)

if not exist "%ROOT%\installer\EvolveMusic.iss" (
    echo ERROR: installer\EvolveMusic.iss is missing.
    exit /b 1
)

if not exist "%DIST%" mkdir "%DIST%"

echo.
echo [2/2] Building installer from staged files...
"%ISCC%" /DMyAppVersion=%VERSION% "%ROOT%\installer\EvolveMusic.iss"
if errorlevel 1 exit /b 1

set "SETUP=%DIST%\EvolveMusic-Setup-%VERSION%.exe"
if not exist "%SETUP%" (
    echo ERROR: Installer finished but setup EXE was not found:
    echo   %SETUP%
    exit /b 1
)

echo.
echo ============================================================
echo INSTALLER SUCCESS
echo EXE: %SETUP%
echo ============================================================
echo.
echo SHA-256:
certutil -hashfile "%SETUP%" SHA256 | findstr /R /V "hash CertUtil"
echo.
exit /b 0
