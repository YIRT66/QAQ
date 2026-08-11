@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\build-mingw"
set "TEMP_DIR=%ROOT%\.tmp"
if not defined EVOLVE_BUILD_JOBS set "EVOLVE_BUILD_JOBS=1"

if not exist "%TEMP_DIR%" mkdir "%TEMP_DIR%"
set "TEMP=%TEMP_DIR%"
set "TMP=%TEMP_DIR%"

rem Source-integrity preflight. Keep this version-agnostic so a valid source
rem tree is not rejected merely because a version marker changed.
if not exist "%ROOT%\CMakeLists.txt" (
    echo ERROR: CMakeLists.txt is missing from the EvolveMusic source root.
    echo Expected: %ROOT%\CMakeLists.txt
    exit /b 1
)
findstr /C:"project(EvolveMusic" "%ROOT%\CMakeLists.txt" >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMakeLists.txt is not an EvolveMusic project file.
    echo Expected source root: %ROOT%
    exit /b 1
)
findstr /C:"class CachedNetworkAccessManagerFactory final" "%ROOT%\core\CachedNetworkAccessManagerFactory.h" >nul 2>nul
if errorlevel 1 (
    echo ERROR: core\CachedNetworkAccessManagerFactory.h is missing or corrupted.
    echo Re-extract the v0.18.1 ZIP and overwrite this file.
    exit /b 1
)

rem v0.18.1 source guard: refuse to build a stale UI tree.
findstr /C:"project(EvolveMusic VERSION 0.18.1" "%ROOT%\CMakeLists.txt" >nul 2>nul
if errorlevel 1 (
    echo ERROR: Source version mismatch. Expected EvolveMusic v0.18.1.
    echo This usually means the ZIP was extracted into a nested folder or old files were not overwritten.
    echo Expected file: %ROOT%\CMakeLists.txt
    exit /b 1
)

rem v0.18.1 music-backend guard: catalog/search/streaming must use
rem Yuncan050115/ourcraft-music-api, while Evolve Cloud remains social/account only.
if not exist "%ROOT%\providers\OurcraftProvider.cpp" (
    echo ERROR: providers\OurcraftProvider.cpp is missing.
    echo Re-extract the v0.18.1 ZIP directly into %ROOT% and choose Replace all.
    exit /b 1
)
findstr /C:"https://music.yuncan.xyz" "%ROOT%\providers\OurcraftProvider.cpp" >nul 2>nul
if errorlevel 1 (
    echo ERROR: Ourcraft default API endpoint is missing from OurcraftProvider.cpp.
    exit /b 1
)
findstr /C:"providers/OurcraftProvider.cpp" "%ROOT%\CMakeLists.txt" >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMakeLists.txt does not compile OurcraftProvider.cpp.
    exit /b 1
)
findstr /C:"m_sources.resolveBestStream(track, m_qualityLevel);" "%ROOT%\core\AppController.cpp" >nul 2>nul
if errorlevel 1 (
    echo ERROR: Ourcraft playback routing is missing from AppController.cpp.
    exit /b 1
)
findstr /C:"m_cloudMusic.loadRoam(" "%ROOT%\core\AppController.cpp" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: Legacy Evolve Cloud music recommendation call is still present.
    echo v0.18.1 requires Ourcraft for music catalog/recommendation/playback.
    exit /b 1
)
findstr /C:"CloudMusicClient::streamReady" "%ROOT%\core\AppController.cpp" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: Legacy Evolve Cloud stream signal is still wired into AppController.
    exit /b 1
)
findstr /C:"CloudMusicClient::searchReady" "%ROOT%\core\AppController.cpp" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: Legacy Evolve Cloud music-search signal is still wired into AppController.
    exit /b 1
)
rem v0.18.1 cloud bootstrap reentrancy guard. A persisted auth token can emit
rem authChanged while CloudMusicClient is still starting; social runWhenReady calls
rem must never recursively enter initialize().
findstr /C:"bool m_initializeInFlight = false;" "%ROOT%\core\CloudMusicClient.h" >nul 2>nul
if errorlevel 1 (
    echo ERROR: CloudMusicClient bootstrap reentrancy guard is missing.
    echo Re-extract the v0.18.1 ZIP directly into %ROOT% and choose Replace all.
    exit /b 1
)
findstr /C:"if (m_initializeInFlight)" "%ROOT%\core\CloudMusicClient.cpp" >nul 2>nul
if errorlevel 1 (
    echo ERROR: CloudMusicClient initialize guard implementation is missing.
    exit /b 1
)
rem Qt signal names must not be shadowed by local variables. In v0.16.4,
rem `const bool identityChanged/authChanged` hid the signals and made MinGW
rem reject `emit identityChanged()` / `emit authChanged()`.
findstr /C:"const bool identityChanged =" "%ROOT%\core\CloudMusicClient.cpp" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: CloudMusicClient.cpp still shadows the identityChanged Qt signal.
    echo Expected v0.18.1 identityDidChange/authDidChange implementation.
    exit /b 1
)
findstr /C:"const bool authChanged =" "%ROOT%\core\CloudMusicClient.cpp" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: CloudMusicClient.cpp still shadows the authChanged Qt signal.
    echo Expected v0.18.1 identityDidChange/authDidChange implementation.
    exit /b 1
)

findstr /C:"property int sidePadding: 16" "%ROOT%\components\AppButton.qml" >nul 2>nul
if errorlevel 1 (
    echo ERROR: components\AppButton.qml is stale or was not overwritten.
    echo Expected the v0.18.1 sidePadding implementation.
    echo File: %ROOT%\components\AppButton.qml
    exit /b 1
)
findstr /C:"property int horizontalPadding:" "%ROOT%\components\AppButton.qml" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: Old v0.15.7 AppButton.qml detected.
    echo The old file redeclares Qt FINAL property horizontalPadding and cannot start.
    echo Re-extract the v0.18.1 ZIP directly into %ROOT% and choose Replace all.
    exit /b 1
)

rem v0.18.1 font safety guard: application-wide font mutation is only allowed once,
rem in AppController construction before QQmlApplicationEngine loads Main.qml.
for /f %%C in ('findstr /C:"QGuiApplication::setFont(font);" "%ROOT%\core\AppController.cpp" ^| find /C ":"') do set "FONT_SETFONT_COUNT=%%C"
if not "%FONT_SETFONT_COUNT%"=="1" (
    echo ERROR: Unsafe application font mutation detected.
    echo Expected exactly one startup QGuiApplication::setFont call, found %FONT_SETFONT_COUNT%.
    echo This guard prevents the Qt 6.10 QML recursive font invalidation crash.
    exit /b 1
)

rem Live QML font bindings are forbidden. Font selection is device-local and restart-only;
rem runtime font reactivity previously caused recursive QML invalidation on Qt 6.10/Windows.
findstr /C:"font.family: app.resolvedFontFamily" "%ROOT%\Main.qml" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: Unsafe live ApplicationWindow font binding detected in Main.qml.
    echo Remove font.family: app.resolvedFontFamily and apply fonts only before QML startup.
    exit /b 1
)
findstr /C:"font.family: app.fontFamily" "%ROOT%\Main.qml" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: Unsafe live ApplicationWindow font binding detected in Main.qml.
    exit /b 1
)

rem v0.18.1: font selector must remain restart-only and non-reactive in QML.
findstr /C:"NOTIFY fontFamilyChanged" "%ROOT%\core\AppController.h" >nul 2>nul
if not errorlevel 1 (
    echo ERROR: Unsafe runtime font NOTIFY signal detected.
    echo Font changes must be restart-only on Qt 6.10/Windows.
    exit /b 1
)
findstr /C:"Component.onCompleted: currentIndex = Math.max(0, app.availableFonts.indexOf(app.fontFamily))" "%ROOT%\pages\SettingsPage.qml" >nul 2>nul
if errorlevel 1 (
    echo ERROR: SettingsPage font selector lost the restart-only initialization guard.
    exit /b 1
)

rem v0.18.1 community reliability guard: list refreshes must be deduplicated.
findstr /C:"bool m_communityListInFlight = false;" "%ROOT%\core\CloudMusicClient.h" >nul 2>nul
if errorlevel 1 (
    echo ERROR: Community request in-flight guard is missing.
    exit /b 1
)
findstr /C:"if (m_communityListInFlight)" "%ROOT%\core\CloudMusicClient.cpp" >nul 2>nul
if errorlevel 1 (
    echo ERROR: Community request deduplication implementation is missing.
    exit /b 1
)

call "%~dp0qt_mingw_env.bat"
if errorlevel 1 exit /b 1

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake.exe was not found on PATH.
    exit /b 1
)

if /I "%~1"=="clea" set "EVOLVE_FORCE_CLEAN=1"
if /I "%~1"=="clean" set "EVOLVE_FORCE_CLEAN=1"

if defined EVOLVE_FORCE_CLEAN (
    echo [EvolveMusic] Checking whether EvolveMusic.exe is still running before clean...
    tasklist /FI "IMAGENAME eq EvolveMusic.exe" 2>nul | find /I "EvolveMusic.exe" >nul
    if not errorlevel 1 (
        echo [EvolveMusic] Closing running EvolveMusic.exe before deleting build files...
        taskkill /F /IM EvolveMusic.exe >nul 2>nul
        timeout /t 1 /nobreak >nul
    )
    echo [EvolveMusic] Cleaning MinGW build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "%BUILD_DIR%" (
        echo ERROR: Could not remove %BUILD_DIR%.
        echo Close any EvolveMusic.exe, Explorer preview, antivirus scan, or terminal holding files in that folder and retry.
        exit /b 1
    )
)

if exist "%BUILD_DIR%\CMakeCache.txt" (
    findstr /C:"CMAKE_GENERATOR:INTERNAL=MinGW Makefiles" "%BUILD_DIR%\CMakeCache.txt" >nul 2>nul
    if errorlevel 1 (
        echo [EvolveMusic] Old CMake generator detected. Recreating build-mingw...
        rmdir /s /q "%BUILD_DIR%"
    ) else (
        findstr /C:"EVOLVE_ENABLE_QML_CACHEGEN:BOOL=OFF" "%BUILD_DIR%\CMakeCache.txt" >nul 2>nul
        if errorlevel 1 (
            echo [EvolveMusic] Old QML AOT build detected.
            echo [EvolveMusic] Recreating build-mingw in low-memory QML mode...
            rmdir /s /q "%BUILD_DIR%"
        )
    )
)

if not exist "%ROOT%\third_party\EvolveUI\components\ETheme.qml" (
    call "%~dp0setup_evolveui.bat"
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo   EvolveMusic - Qt MinGW build
echo ============================================================
echo Qt:       %QT_ROOT%
if defined MINGW_ROOT (
    echo MinGW:    %MINGW_ROOT%
) else (
    echo Compiler: %GXX%
)
echo Build:    %BUILD_DIR%
echo Temp:     %TEMP_DIR%
echo Jobs:     %EVOLVE_BUILD_JOBS% ^(single-job default^)
echo QML AOT:  DISABLED ^(avoids MinGW ICE in generated *_qml.cpp files^)
if exist "%QT_ROOT%\lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake" (echo WebLogin: ENABLED - Qt WebView / Edge WebView2) else (echo WebLogin: DISABLED - Qt WebView missing)
echo ============================================================
echo.

echo [1/2] Configuring...
cmake -S "%ROOT%" -B "%BUILD_DIR%" ^
  -G "MinGW Makefiles" ^
  "-DCMAKE_PREFIX_PATH=%QT_ROOT%" ^
  "-DCMAKE_CXX_COMPILER=%GXX%" ^
  "-DCMAKE_MAKE_PROGRAM=%MAKE_EXE%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DEVOLVE_ENABLE_QML_CACHEGEN=OFF ^
  "-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG"

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed.
    echo.
    echo Useful diagnostics:
    echo   "%QT_ROOT%\bin\qmake.exe" --version
    echo   "%GXX%" --version
    echo   "%MAKE_EXE%" --version
    echo.
    echo To force a clean configure:
    echo   scripts\build_windows.bat clean
    exit /b 1
)

echo.
echo [EvolveMusic] Checking whether EvolveMusic.exe is still running...
tasklist /FI "IMAGENAME eq EvolveMusic.exe" 2>nul | find /I "EvolveMusic.exe" >nul
if not errorlevel 1 (
    echo [EvolveMusic] Closing running EvolveMusic.exe so the linker can replace it...
    taskkill /F /IM EvolveMusic.exe >nul 2>nul
    timeout /t 1 /nobreak >nul
)

echo.
echo [2/2] Building...
cmake --build "%BUILD_DIR%" --parallel %EVOLVE_BUILD_JOBS%
if errorlevel 1 (
    echo.
    echo ERROR: Compilation failed.
    exit /b 1
)

set "EXE=%BUILD_DIR%\EvolveMusic.exe"
if not exist "%EXE%" (
    echo ERROR: Build finished but EvolveMusic.exe was not found:
    echo   %EXE%
    exit /b 1
)

echo.
echo ============================================================
echo BUILD SUCCESS
echo EXE: %EXE%
echo ============================================================
exit /b 0
