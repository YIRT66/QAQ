param(
    [string]$TwoT58Keyword = "差一步",
    [string]$YuetingKeyword = "晴天"
)

$ErrorActionPreference = "Continue"
$ProgressPreference = "SilentlyContinue"

$ua147 = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36"
$cookie = $env:EVOLVE_2T58_COOKIE

if (-not $cookie) {
    $cookieFile = Join-Path (Split-Path $PSScriptRoot -Parent) "runtime\client-data\2t58-cookie.txt"
    if (Test-Path $cookieFile) {
        $cookie = (Get-Content -Raw $cookieFile).Trim()
    }
}

if (-not $cookie) {
    $cookie = "Hm_tf_hx9umupwu8o=1766942296; 9be49c0fcbd87e6a36f944af3f638e63=701e0362d41fe970a431ab4e7e0a8260; server_name_session=8e658a40df8491e40010dc3307caacde; Hm_lvt_hx9umupwu8o=1775811750,1776490879; Hm_lpvt_hx9umupwu8o=1776492031"
}

function CurlPage([string]$Url, [bool]$Use2t58Headers = $false) {
    $args = @(
        "--location",
        "--silent",
        "--show-error",
        "--compressed",
        "--connect-timeout", "2",
        "--max-time", "6",
        "--user-agent", $(if ($Use2t58Headers) { $ua147 } else { "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131 Safari/537.36" }),
        "--header", "Accept-Language: zh-CN,zh;q=0.9,en-US;q=0.8,en;q=0.7"
    )

    if ($Use2t58Headers) {
        $args += @(
            "--referer", "https://www.2t58.com/",
            "--header", "Cookie: $cookie",
            "--header", 'Sec-CH-UA: "Google Chrome";v="147", "Not.A/Brand";v="8", "Chromium";v="147"',
            "--header", 'Sec-CH-UA-Mobile: ?0',
            "--header", 'Sec-CH-UA-Platform: "Windows"',
            "--header", 'Sec-Fetch-Dest: document',
            "--header", 'Sec-Fetch-Mode: navigate',
            "--header", 'Sec-Fetch-Site: same-origin',
            "--header", 'Sec-Fetch-User: ?1'
        )
    }

    $args += $Url
    return (& curl.exe @args 2>$null | Out-String)
}

Write-Host "============================================================"
Write-Host " EvolveMusic v0.8.8 Source Diagnostic"
Write-Host "============================================================"

$yEncoded = [uri]::EscapeDataString($YuetingKeyword)
$yUrl = "https://www.yueting.net/so?nsid=4&q=$yEncoded"
$y = CurlPage $yUrl $false
$yCount = [regex]::Matches($y, '/song/(\d+)(?:\.html)?', 'IgnoreCase').Count

Write-Host ""
Write-Host "[Yueting known-good search: $YuetingKeyword]" -ForegroundColor Cyan
Write-Host $yUrl
Write-Host ("chars={0} songLinks={1}" -f $y.Length, $yCount)

$tEncoded = [uri]::EscapeDataString($TwoT58Keyword)
$tUrl = "https://www.2t58.com/so/$tEncoded.html"

$tPlain = CurlPage $tUrl $false
$tPlainCount = [regex]::Matches($tPlain, '/song/([A-Za-z0-9_-]+)(?:\.html)?', 'IgnoreCase').Count

$tCompat = CurlPage $tUrl $true
$tCompatCount = [regex]::Matches($tCompat, '/song/([A-Za-z0-9_-]+)(?:\.html)?', 'IgnoreCase').Count
$playListPresent = $tCompat -match 'play_list'

Write-Host ""
Write-Host "[2t58: $TwoT58Keyword]" -ForegroundColor Cyan
Write-Host $tUrl
Write-Host ("plain:  chars={0} songLinks={1}" -f $tPlain.Length, $tPlainCount)
Write-Host ("compat: chars={0} songLinks={1} play_list={2}" -f $tCompat.Length, $tCompatCount, $playListPresent)

if ($tCompatCount -gt 0) {
    Write-Host "[OK] 2t58 compatibility headers/cookie returned the real result list." -ForegroundColor Green
} elseif ($tPlainCount -eq 0 -and $tCompatCount -eq 0) {
    Write-Host "[WARN] 2t58 still returned its reduced page; the site cookie may have rotated." -ForegroundColor Yellow
    Write-Host "You can override it without recompiling:" -ForegroundColor Yellow
    Write-Host "  runtime\client-data\2t58-cookie.txt"
    Write-Host "or environment variable EVOLVE_2T58_COOKIE"
}

Write-Host ""
Write-Host "Important: Yueting '$TwoT58Keyword' is not used as its health baseline."
Write-Host "The provider health baseline is '$YuetingKeyword', a song currently known to exist on Yueting."
