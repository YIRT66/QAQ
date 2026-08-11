param(
    [string]$QtRoot = "E:\QT\6.10.0\mingw_64",
    [string]$ProjectRoot = "F:\EvolveMusic"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Write-Step([string]$Text) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor DarkGray
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor DarkGray
}

function Fail([string]$Text) {
    Write-Host ""
    Write-Host "ERROR: $Text" -ForegroundColor Red
    exit 1
}

function Find-Extractor {
    foreach ($name in @("7z.exe", "7za.exe", "7zr.exe")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) {
            return @{ Type = "7z"; Path = $cmd.Source }
        }
    }

    # Look in a few common Qt / 7-Zip locations without scanning the whole drive.
    $candidates = @(
        "E:\QT\Tools\7zip\7z.exe",
        "E:\QT\Tools\7-Zip\7z.exe",
        "C:\Program Files\7-Zip\7z.exe",
        "C:\Program Files (x86)\7-Zip\7z.exe"
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) {
            return @{ Type = "7z"; Path = $p }
        }
    }

    $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
    if ($tar) {
        return @{ Type = "tar"; Path = $tar.Source }
    }

    return $null
}

Write-Step "EvolveMusic - Qt WebView 6.10.0 MinGW Installer"

$qmake = Join-Path $QtRoot "bin\qmake.exe"
if (!(Test-Path $qmake)) {
    Fail "Qt was not found at: $QtRoot`nExpected: $qmake"
}

$qtVersion = (& $qmake -query QT_VERSION).Trim()
$qmakeSpec = (& $qmake -query QMAKE_XSPEC).Trim()

Write-Host "Qt root    : $QtRoot"
Write-Host "Qt version : $qtVersion"
Write-Host "Qt spec    : $qmakeSpec"

if ($qtVersion -ne "6.10.0") {
    Fail "This installer package is prepared for Qt 6.10.0. Detected: $qtVersion"
}
# Qt's MinGW kit commonly reports QMAKE_XSPEC as "win32-g++",
# not a string containing the word "mingw".
$looksLikeMinGW = ($qmakeSpec -match "mingw") -or ($qmakeSpec -match "g\+\+") -or ($qmakeSpec -eq "win32-g++")
if (-not $looksLikeMinGW) {
    Fail "This installer requires the Qt MinGW kit. Detected QMAKE_XSPEC: $qmakeSpec"
}

$configFile = Join-Path $QtRoot "lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake"
$qmlDir = Join-Path $QtRoot "qml\QtWebView"

if ((Test-Path $configFile) -and (Test-Path $qmlDir)) {
    Write-Host ""
    Write-Host "[OK] Qt WebView is already installed." -ForegroundColor Green
    Write-Host "CMake : $configFile"
    Write-Host "QML   : $qmlDir"
    exit 0
}

# Qt official online repository for Qt 6.10.0 / MinGW 64-bit.
$repo = "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6100/qt6_6100/qt.qt6.6100.addons.qtwebview.win64_mingw/"

# Keep temporary files off C: because this machine is low on C: space.
if (Test-Path "F:\") {
    $cacheRoot = "F:\QtWebViewInstallerCache"
} else {
    $qtBase = Split-Path (Split-Path $QtRoot -Parent) -Parent
    $cacheRoot = Join-Path $qtBase "_QtWebViewInstallerCache"
}

$downloadDir = Join-Path $cacheRoot "download"
$stageDir = Join-Path $cacheRoot "stage"

if (Test-Path $stageDir) {
    Remove-Item $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Write-Step "1/5 Discovering package from Qt official repository"
Write-Host "Repository: $repo"

try {
    $page = Invoke-WebRequest -Uri $repo -UseBasicParsing
} catch {
    Fail "Could not access Qt official repository.`n$($_.Exception.Message)"
}

# Match the actual archive, not .sha1/.meta files.
$matches = [regex]::Matches(
    $page.Content,
    'href="([^"]*6\.10\.0[^"]*qtwebview[^"]*Mingw[^"]*X86_64\.7z)"',
    [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
)

if ($matches.Count -eq 0) {
    # Fallback to the known official Qt 6.10.0 MinGW package name.
    $archiveName = "6.10.0-0-202510021201qtwebview-Windows-Windows_10_22H2-Mingw-Windows-Windows_10_22H2-X86_64.7z"
    Write-Host "Index parsing did not find the archive; using the verified Qt 6.10.0 package name." -ForegroundColor Yellow
} else {
    $archiveName = [System.Net.WebUtility]::HtmlDecode($matches[0].Groups[1].Value)
}

$archiveName = Split-Path $archiveName -Leaf
$archiveUrl = $repo + $archiveName
$sha1Url = $archiveUrl + ".sha1"
$archivePath = Join-Path $downloadDir $archiveName
$sha1Path = $archivePath + ".sha1"

Write-Host "Archive: $archiveName"

Write-Step "2/5 Downloading Qt WebView"
try {
    Invoke-WebRequest -Uri $archiveUrl -OutFile $archivePath -UseBasicParsing
    Invoke-WebRequest -Uri $sha1Url -OutFile $sha1Path -UseBasicParsing
} catch {
    Fail "Download failed.`n$($_.Exception.Message)`nURL: $archiveUrl"
}

if (!(Test-Path $archivePath)) {
    Fail "Archive was not downloaded."
}

Write-Step "3/5 Verifying official SHA-1"
$expected = ((Get-Content $sha1Path -Raw).Trim() -split '\s+')[0].ToLowerInvariant()
$actual = (Get-FileHash -Path $archivePath -Algorithm SHA1).Hash.ToLowerInvariant()

Write-Host "Expected: $expected"
Write-Host "Actual  : $actual"

if (!$expected -or $expected -ne $actual) {
    Fail "SHA-1 verification failed. The archive will NOT be installed."
}
Write-Host "[OK] Package hash verified." -ForegroundColor Green

Write-Step "4/5 Extracting and installing"
$extractor = Find-Extractor
if (!$extractor) {
    Fail "No 7-Zip or Windows tar.exe extractor was found.`nInstall 7-Zip, or ensure C:\Windows\System32\tar.exe exists."
}

Write-Host "Extractor: $($extractor.Path)"

if ($extractor.Type -eq "7z") {
    & $extractor.Path x "-o$stageDir" -y $archivePath | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Fail "7-Zip extraction failed with exit code $LASTEXITCODE."
    }
} else {
    & $extractor.Path -xf $archivePath -C $stageDir
    if ($LASTEXITCODE -ne 0) {
        Fail "tar.exe could not extract the Qt .7z archive. Install 7-Zip and run this installer again."
    }
}

# Discover the actual Qt kit root inside the online-installer archive instead of
# assuming a fixed top-level folder layout.
$foundConfig = Get-ChildItem -Path $stageDir -Recurse -File -Filter "Qt6WebViewConfig.cmake" |
    Select-Object -First 1

if (!$foundConfig) {
    Fail "Extraction completed, but Qt6WebViewConfig.cmake was not found. The package layout was unexpected."
}

# ...\<kit-root>\lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake
$packageQtRoot = $foundConfig.Directory.Parent.Parent.Parent.FullName

Write-Host "Package Qt root: $packageQtRoot"
Write-Host "Target Qt root : $QtRoot"

# Copy package contents into the existing 6.10.0 MinGW kit.
# robocopy exit codes 0..7 are success/nonfatal.
& robocopy $packageQtRoot $QtRoot /E /COPY:DAT /DCOPY:DAT /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
$rc = $LASTEXITCODE
if ($rc -gt 7) {
    Fail "robocopy failed with exit code $rc."
}

Write-Step "5/5 Verifying installation"
$checks = @(
    (Join-Path $QtRoot "lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake"),
    (Join-Path $QtRoot "qml\QtWebView\qmldir")
)

$failed = $false
foreach ($c in $checks) {
    if (Test-Path $c) {
        Write-Host "[OK] $c" -ForegroundColor Green
    } else {
        Write-Host "[MISSING] $c" -ForegroundColor Red
        $failed = $true
    }
}

$dll = Get-ChildItem -Path (Join-Path $QtRoot "bin") -Filter "Qt6WebView*.dll" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($dll) {
    Write-Host "[OK] $($dll.FullName)" -ForegroundColor Green
}

if ($failed) {
    Fail "Qt WebView installation verification failed."
}

# Optional EvolveMusic check.
$checkScript = Join-Path $ProjectRoot "scripts\check_webview.bat"
if (Test-Path $checkScript) {
    Write-Host ""
    Write-Host "Running EvolveMusic WebView check..."
    & $checkScript
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host " QT WEBVIEW INSTALL SUCCESS" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next command:"
Write-Host "  cd /d $ProjectRoot"
Write-Host "  scripts\build_windows.bat clean"
Write-Host ""
Write-Host "Temporary download cache:"
Write-Host "  $cacheRoot"
Write-Host "You can delete that cache after a successful build."
