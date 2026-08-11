@echo off
setlocal EnableExtensions

title EvolveMusic v0.11.3 Source + Path Fix

set "TARGET=F:\EvolveMusic"
if not "%~1"=="" set "TARGET=%~1"

echo ============================================================
echo   EvolveMusic v0.11.3 - SOURCE + PATH FIX
echo ============================================================
echo Target: %TARGET%
echo.

REM Use stable pre-existing project files as the project identity check.
if not exist "%TARGET%\core\AppController.h" (
    echo [FAIL] EvolveMusic project not found.
    echo Missing:
    echo   %TARGET%\core\AppController.h
    echo.
    echo If your project is elsewhere, run:
    echo   INSTALL_FIX.bat "X:\Path\To\EvolveMusic"
    pause
    exit /b 1
)

if not exist "%~dp0core\CloudPolicyClient.h" (
    echo [FAIL] Repair package is incomplete: CloudPolicyClient.h missing.
    pause
    exit /b 1
)

echo [1/6] Backing up current CMakeLists.txt...
if exist "%TARGET%\CMakeLists.txt" (
    copy /Y "%TARGET%\CMakeLists.txt" "%TARGET%\CMakeLists.txt.before-v0.11.3.bak" >nul
    if errorlevel 1 (
        echo [FAIL] Could not back up CMakeLists.txt.
        pause
        exit /b 1
    )
)

echo [2/6] Installing missing CloudPolicyClient sources...
if not exist "%TARGET%\core\" mkdir "%TARGET%\core"
copy /Y "%~dp0core\CloudPolicyClient.h" "%TARGET%\core\CloudPolicyClient.h" >nul
if errorlevel 1 goto :copyfail
copy /Y "%~dp0core\CloudPolicyClient.cpp" "%TARGET%\core\CloudPolicyClient.cpp" >nul
if errorlevel 1 goto :copyfail

echo [3/6] Refreshing CloudMusicClient sources...
copy /Y "%~dp0core\CloudMusicClient.h" "%TARGET%\core\CloudMusicClient.h" >nul
if errorlevel 1 goto :copyfail
copy /Y "%~dp0core\CloudMusicClient.cpp" "%TARGET%\core\CloudMusicClient.cpp" >nul
if errorlevel 1 goto :copyfail

echo [4/6] Installing corrected absolute-path CMakeLists.txt...
copy /Y "%~dp0CMakeLists.txt" "%TARGET%\CMakeLists.txt" >nul
if errorlevel 1 goto :copyfail

echo [5/6] Verifying installation...
if not exist "%TARGET%\core\CloudPolicyClient.h" (
    echo [FAIL] CloudPolicyClient.h was not installed.
    pause
    exit /b 1
)
if not exist "%TARGET%\core\CloudPolicyClient.cpp" (
    echo [FAIL] CloudPolicyClient.cpp was not installed.
    pause
    exit /b 1
)

findstr /C:"VERSION 0.11.3" "%TARGET%\CMakeLists.txt" >nul
if errorlevel 1 (
    echo [FAIL] v0.11.3 CMakeLists.txt was not installed.
    pause
    exit /b 1
)

findstr /C:"EVOLVEMUSIC_SOURCE_ROOT" "%TARGET%\CMakeLists.txt" >nul
if errorlevel 1 (
    echo [FAIL] Absolute source path fix is missing.
    pause
    exit /b 1
)

echo [6/6] Removing stale MinGW build directory...
if exist "%TARGET%\build-mingw\" (
    rmdir /S /Q "%TARGET%\build-mingw"
)
if exist "%TARGET%\build-mingw\" (
    echo [FAIL] Could not delete build-mingw.
    echo Close EvolveMusic.exe, Qt Creator, CMake, or terminals using that folder.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo [OK] v0.11.3 repair installed.
echo ============================================================
echo.
echo Added:
echo   %TARGET%\core\CloudPolicyClient.h
echo   %TARGET%\core\CloudPolicyClient.cpp
echo.
echo Updated:
echo   %TARGET%\core\CloudMusicClient.h
echo   %TARGET%\core\CloudMusicClient.cpp
echo   %TARGET%\CMakeLists.txt
echo.
echo Now run:
echo   cd /d %TARGET%
echo   scripts\build_windows.bat clean
echo.
echo During configure, look for:
echo   EvolveMusic v0.11.3 SOURCE+PATH FIX: ACTIVE
echo   EvolveMusic source root: F:/EvolveMusic
echo   EvolveMusic binary root: F:/EvolveMusic/build-mingw
echo.
pause
exit /b 0

:copyfail
echo [FAIL] Could not copy repair files into:
echo   %TARGET%
pause
exit /b 1
