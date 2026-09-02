# Copies the built mod into the game folder (Binaries\Win32 next to
# Dishonored.exe): d3d9.dll (the proxy).
# Backs up a pre-existing d3d9.dll that is not ours as d3d9.dll.dvr-backup once.
# Deliberately NOT guarded against a running game or headset: copying files
# touches the disk only (the copy fails if the DLL is loaded, which is fine).
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [switch]$Release,
    [string]$GamePath = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$repo = Split-Path -Parent $PSScriptRoot
$config = if ($Release) { "RelWithDebInfo" } else { "Debug" }
$outDir = Join-Path $repo "build\src\$config"
$GamePath = Get-DvrGamePath $GamePath

$proxy = Join-Path $outDir "d3d9.dll"
if (-not (Test-Path $proxy)) { throw "Build output missing: $proxy - run tools\build.ps1 first." }
Assert-DvrX86Dll $proxy

# Back up a foreign d3d9.dll exactly once (ours is recognised by the ini/log it writes).
$existing = Join-Path $GamePath "d3d9.dll"
$backup = Join-Path $GamePath "d3d9.dll.dvr-backup"
if ((Test-Path $existing) -and -not (Test-Path $backup)) {
    $ours = (Test-Path (Join-Path $GamePath "dishonored_vr.log")) -or (Test-Path (Join-Path $GamePath "dishonored_vr.ini"))
    if (-not $ours) {
        Copy-Item $existing $backup
        Write-Host "Backed up existing d3d9.dll -> d3d9.dll.dvr-backup"
    }
}

Copy-Item $proxy $existing -Force
# The DXVK fork is gone (41.0). An older install's dxvk_d3d9.dll is loaded by
# nothing any more, but leaving it beside the exe invites confusion.
$oldFork = Join-Path $GamePath "dxvk_d3d9.dll"
if (Test-Path $oldFork) { Remove-Item $oldFork -Force; Write-Host "Removed the retired dxvk_d3d9.dll" }

Write-Host "Installed $config build to $GamePath"
Write-Host "  d3d9.dll        $(Get-Item $proxy | Select-Object -ExpandProperty LastWriteTime)"
Write-Host "Log: $(Join-Path $GamePath 'dishonored_vr.log')   harness files: $(Get-DvrDataDir)"
