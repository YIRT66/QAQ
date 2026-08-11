@echo off
setlocal
set "TARGET=F:\EvolveMusic"
if not "%~1"=="" set "TARGET=%~1"

echo ============================================================
echo   EvolveMusic v0.11.3 verification
echo ============================================================
echo.

for %%F in (
    "core\AppController.h"
    "core\CloudPolicyClient.h"
    "core\CloudPolicyClient.cpp"
    "core\CloudMusicClient.h"
    "core\CloudMusicClient.cpp"
) do (
    if not exist "%TARGET%\%%~F" (
        echo [FAIL] Missing: %TARGET%\%%~F
        exit /b 1
    )
)

findstr /C:"VERSION 0.11.3" "%TARGET%\CMakeLists.txt" >nul
if errorlevel 1 (
    echo [FAIL] CMakeLists.txt is not v0.11.3
    exit /b 1
)

findstr /C:"EVOLVEMUSIC_SOURCE_ROOT" "%TARGET%\CMakeLists.txt" >nul
if errorlevel 1 (
    echo [FAIL] Absolute path fix missing.
    exit /b 1
)

echo [OK] Missing source files are installed.
echo [OK] Absolute source path CMake fix is installed.
exit /b 0
