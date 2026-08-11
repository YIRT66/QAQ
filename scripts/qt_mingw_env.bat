@echo off
rem EvolveMusic Qt/MinGW environment bootstrap.
rem Intentionally no SETLOCAL: variables are exported to the caller.

if not defined EVOLVE_QT_ROOT set "EVOLVE_QT_ROOT=E:\QT\6.10.0\mingw_64"
set "QT_ROOT=%EVOLVE_QT_ROOT%"

rem Qt 6.10 may invoke the Qt License Service even for this open-source build.
rem Qt's own build error explicitly provides this environment switch to bypass
rem that framework license-service check.
set "QTFRAMEWORK_BYPASS_LICENSE_CHECK=1"

if not exist "%QT_ROOT%\bin\qmake.exe" (
    echo ERROR: Qt MinGW kit was not found:
    echo   %QT_ROOT%
    echo.
    echo Set a different Qt path before running this script, for example:
    echo   set "EVOLVE_QT_ROOT=E:\QT\6.10.0\mingw_64"
    exit /b 1
)

for %%I in ("%QT_ROOT%\..\..") do set "QT_BASE=%%~fI"

set "MINGW_ROOT="
for /d %%G in ("%QT_BASE%\Tools\mingw*_64") do (
    if exist "%%~fG\bin\g++.exe" if exist "%%~fG\bin\mingw32-make.exe" set "MINGW_ROOT=%%~fG"
)

set "GXX="
set "MAKE_EXE="
set "MINGW_BIN="

if defined MINGW_ROOT (
    set "GXX=%MINGW_ROOT%\bin\g++.exe"
    set "MAKE_EXE=%MINGW_ROOT%\bin\mingw32-make.exe"
    set "MINGW_BIN=%MINGW_ROOT%\bin"
) else (
    for /f "delims=" %%G in ('where g++ 2^>nul') do if not defined GXX set "GXX=%%G"
    for /f "delims=" %%M in ('where mingw32-make 2^>nul') do if not defined MAKE_EXE set "MAKE_EXE=%%M"

    if not defined GXX (
        echo ERROR: g++.exe was not found.
        echo Install the MinGW toolchain from Qt Maintenance Tool or add MinGW to PATH.
        exit /b 1
    )
    if not defined MAKE_EXE (
        echo ERROR: mingw32-make.exe was not found.
        echo Install the MinGW toolchain from Qt Maintenance Tool or add MinGW to PATH.
        exit /b 1
    )

    for %%I in ("%GXX%") do set "MINGW_BIN=%%~dpI"
    echo WARNING: Qt-bundled MinGW was not found under:
    echo   %QT_BASE%\Tools
    echo Falling back to the MinGW already available on PATH:
    echo   %GXX%
    echo.
)

set "PATH=%QT_ROOT%\bin;%MINGW_BIN%;%PATH%"
exit /b 0
