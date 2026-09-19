# Archive the current install, then install the freshly built Release proxy.
#
# The one-button path for a tester: it refuses while the game is running (the
# DLL is locked and a half-install is worse than none), copies the live log,
# the previous log, the installed DLL and the installed INI somewhere safe
# BEFORE overwriting anything, and leaves dishonored_vr.ini alone so F10
# settings and the runtime selection survive.
#
# It does not launch the game. It does not build. Run tools\build.ps1 -Release
# first; this refuses if the built DLL is older than the newest source file.
param([switch]$SkipFreshnessCheck)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

$running = Get-Process -Name 'Dishonored' -ErrorAction SilentlyContinue
if ($running) {
    Write-Output 'REFUSED: Dishonored.exe is running, so d3d9.dll is locked.'
    Write-Output 'Quit the game all the way to the desktop, then run this again.'
    exit 1
}

$built = Join-Path $repo 'build\src\RelWithDebInfo\d3d9.dll'
if (-not (Test-Path $built)) {
    Write-Output "REFUSED: no Release build at $built. Run .\tools\build.ps1 -Release first."
    exit 1
}
if (-not $SkipFreshnessCheck) {
    $newestSource = Get-ChildItem (Join-Path $repo 'src') -Recurse -Include *.cpp,*.h,*.inc |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($newestSource -and $newestSource.LastWriteTime -gt (Get-Item $built).LastWriteTime) {
        Write-Output "REFUSED: $($newestSource.Name) is newer than the built d3d9.dll."
        Write-Output 'Run .\tools\build.ps1 -Release first, or pass -SkipFreshnessCheck.'
        exit 1
    }
}

. (Join-Path $PSScriptRoot 'lib\game-path.ps1')
$game = Get-DvrGamePath

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$arc = Join-Path $repo "build\playtest-candidates\installs\$stamp"
New-Item -ItemType Directory -Force -Path $arc | Out-Null
foreach ($f in @('d3d9.dll','dishonored_vr.ini','dishonored_vr.log','dishonored_vr.prev.log','dishonored_vr_crash.txt')) {
    $src = Join-Path $game $f
    if (Test-Path $src) { Copy-Item $src $arc -Force }
}
Write-Output "archived the previous install and both logs -> $arc"

# install.ps1 is a script, not a native command: it THROWS on failure and does
# not set $LASTEXITCODE, so testing that variable reads whatever native command
# ran last (it reported a failure on a successful install once already).
try { & (Join-Path $PSScriptRoot 'install.ps1') -Release }
catch {
    Write-Output "install.ps1 failed: $($_.Exception.Message)"
    Write-Output 'The archive above still holds the previous install.'
    exit 1
}

# Say what actually landed, so the banner in the next log can be checked against it.
$dll = Get-Item (Join-Path $game 'd3d9.dll')
$hash = (Get-FileHash $dll.FullName -Algorithm SHA256).Hash
Write-Output ''
Write-Output "installed d3d9.dll  $($dll.Length) bytes  $($dll.LastWriteTime)"
Write-Output "sha256 $hash"
$ini = Join-Path $game 'dishonored_vr.ini'
if (Test-Path $ini) {
    $keys = Get-Content $ini | Select-String '^Version=|^NativeAwarenessMarkers=|^NativeHeartAllSymbols=|^Runtime=|^RenderWidth=|^RenderHeight='
    Write-Output 'dishonored_vr.ini left untouched; the settings that matter for this candidate:'
    $keys | ForEach-Object { Write-Output "  $_" }
}
Write-Output ''
Write-Output 'Ready. Launch the game yourself when you want to test.'
exit 0
