# Build a release zip from the CURRENT tree, reproducibly.
# Reads the version from CMakeLists.txt so the zip name, the DLL banner and the
# tag cannot disagree. Stages an explicit file list: d3d9.dll (proxy),
# openvr_api.dll (Valve, hash-checked), the user docs and the game-ini setup
# script. The simulator never ships.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [string]$OutDir = "$PSScriptRoot\..\dist",
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$repo = Split-Path -Parent $PSScriptRoot

$cml = Get-Content "$repo\CMakeLists.txt" -Raw
if ($cml -notmatch 'project\(DishonoredVR\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw "could not read VERSION from CMakeLists.txt"
}
$version = $Matches[1]
"version: $version"

if (-not $SkipBuild) { & "$repo\tools\build.ps1" -Release }

$bin = "$repo\build\src\RelWithDebInfo"
if (-not (Test-Path "$bin\d3d9.dll")) { throw "missing build output: $bin\d3d9.dll" }
Assert-DvrX86Dll "$bin\d3d9.dll"

$ovrDll = "$repo\third_party\openvr_headers\bin\win32\openvr_api.dll"
$prov = Get-Content "$repo\third_party\openvr_headers\PROVENANCE.txt" -Raw
$hash = (Get-FileHash $ovrDll -Algorithm SHA256).Hash.ToLower()
if ($prov -notmatch [regex]::Escape($hash)) {
    throw "openvr_api.dll sha256 $hash does not appear in PROVENANCE.txt - refusing to package"
}

$gen = Get-Content "$repo\build\generated\dvr_version.h" -Raw
if ($gen -notmatch '#define\s+DVR_VERSION\s+"([^"]+)"') { throw "cannot read generated version" }
if ($Matches[1] -ne $version) {
    throw "generated header says $($Matches[1]) but CMakeLists.txt says $version - rebuild"
}

& "$repo\tools\exports-check.ps1" "$bin\d3d9.dll"

$stage = "$OutDir\dishonored-vr-v$version"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

Copy-Item "$bin\d3d9.dll" $stage
Copy-Item $ovrDll $stage
Copy-Item "$repo\README.md" "$stage\README.txt"
Copy-Item "$repo\docs\TROUBLESHOOTING.md" "$stage\TROUBLESHOOTING.txt"
Copy-Item "$repo\docs\KNOWN_ISSUES.md" "$stage\KNOWN_ISSUES.txt"
Copy-Item "$repo\release\HOW-TO-USE.txt" "$stage\HOW-TO-USE.txt"
Copy-Item "$repo\tools\setup-game-ini.ps1" "$stage\setup-game-ini.ps1"

$zip = "$OutDir\dishonored-vr-v$version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Remove-Item $stage -Recurse -Force

"packaged: $zip"
"{0} bytes" -f (Get-Item $zip).Length
