@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "API=%ROOT%\runtime\netease-api"

echo [EvolveMusic] Preparing NetEase music source service...

where git >nul 2>nul
if errorlevel 1 (
    echo ERROR: Git is required.
    exit /b 1
)

where node >nul 2>nul
if errorlevel 1 (
    echo ERROR: Node.js 22 or newer is required.
    exit /b 1
)

for /f %%V in ('node -p "parseInt(process.versions.node)"') do set "NODE_MAJOR=%%V"
if %NODE_MAJOR% LSS 22 (
    echo ERROR: Node.js 22 or newer is required. Current major: %NODE_MAJOR%
    exit /b 1
)

if not exist "%API%\app.js" (
    if exist "%API%" rmdir /s /q "%API%"
    if not exist "%ROOT%\runtime" mkdir "%ROOT%\runtime"

    echo [EvolveMusic] Cloning API source...
    git clone --depth 1 https://github.com/NeteaseCloudMusicApiEnhanced/api-enhanced.git "%API%"
    if errorlevel 1 exit /b 1
) else (
    echo [EvolveMusic] API source already exists. Updating...
    git -C "%API%" pull --ff-only
    if errorlevel 1 (
        echo WARNING: Could not update API source. Continuing with the existing copy.
    )
)

if not exist "%API%\package.json" (
    echo ERROR: package.json was not found after cloning:
    echo   %API%\package.json
    exit /b 1
)

pushd "%API%"
if errorlevel 1 (
    echo ERROR: Could not enter API directory:
    echo   %API%
    exit /b 1
)

echo [EvolveMusic] API working directory:
echo   %CD%

where pnpm >nul 2>nul
if not errorlevel 1 (
    rem pnpm is commonly a .cmd shim on Windows. CALL is required from a batch file
    rem so execution returns here instead of abandoning this setup script.
    call pnpm install
    set "ERR=%errorlevel%"
) else (
    where corepack >nul 2>nul
    if errorlevel 1 (
        echo ERROR: pnpm was not found and Corepack is unavailable.
        popd
        exit /b 1
    )

    echo [EvolveMusic] pnpm not found; using Corepack...
    call corepack pnpm install
    set "ERR=%errorlevel%"
)

popd

if not "%ERR%"=="0" (
    echo ERROR: NetEase API dependency installation failed with code %ERR%.
    exit /b %ERR%
)

echo.
echo [EvolveMusic] NetEase API is ready:
echo   %API%
exit /b 0
