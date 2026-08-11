@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\build-mingw"
set "EXE=%BUILD_DIR%\EvolveMusic.exe"
set "DIST=%ROOT%\dist\EvolveMusic"
set "TEMP=%ROOT%\.tmp"
set "TMP=%ROOT%\.tmp"
if not exist "%TEMP%" mkdir "%TEMP%"

call "%~dp0qt_mingw_env.bat"
if errorlevel 1 exit /b 1

call "%~dp0build_windows.bat"
if errorlevel 1 exit /b 1

if not exist "%EXE%" (
    echo ERROR: EvolveMusic.exe was not found:
    echo   %EXE%
    exit /b 1
)

set "WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe"
if not exist "%WINDEPLOYQT%" (
    echo ERROR: windeployqt.exe was not found:
    echo   %WINDEPLOYQT%
    exit /b 1
)

if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"
copy /y "%EXE%" "%DIST%\EvolveMusic.exe" >nul

echo [EvolveMusic] Deploying Qt runtime...
"%WINDEPLOYQT%" --release --compiler-runtime --qmldir "%ROOT%" "%DIST%\EvolveMusic.exe"
if errorlevel 1 exit /b 1

for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_BIN%\%%D" if not exist "%DIST%\%%D" copy /y "%MINGW_BIN%\%%D" "%DIST%\%%D" >nul
)

copy /y "%ROOT%\README.md" "%DIST%\README.md" >nul

echo.
echo ============================================================
echo PACKAGE SUCCESS
echo Folder: %DIST%
echo EXE:    %DIST%\EvolveMusic.exe
echo ============================================================
exit /b 0
