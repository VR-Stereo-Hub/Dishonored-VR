# Builds the mod (32-bit): d3d9.dll (the proxy + VR core), the simulated OpenXR
# runtime, the dvr_steamvr32 SteamVR shim and the xr_hello32 smoke client. CMake
# is not on PATH on this machine;
# we use the one bundled with VS 2022 Build Tools, located via vswhere.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [switch]$Release,
    [switch]$Install,
    [switch]$Legacy,      # also compile src/legacy (retired experiments)
    [string]$GamePath = ""
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install VS 2022 Build Tools." }
$cmake = & $vswhere -latest -products * -find "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" | Select-Object -First 1
if (-not $cmake) { throw "VS-bundled CMake not found - install the 'C++ CMake tools' component." }

# cmake presets are resolved relative to the current directory
Push-Location $repo
try {
    $legacyFlag = if ($Legacy) { "ON" } else { "OFF" }
    if (-not (Test-Path "build\CMakeCache.txt")) {
        & $cmake --preset win32 "-DDVR_WITH_LEGACY=$legacyFlag"
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
    } elseif ($Legacy) {
        & $cmake -S . -B build "-DDVR_WITH_LEGACY=$legacyFlag"
        if ($LASTEXITCODE -ne 0) { throw "CMake reconfigure failed." }
    }

    $preset = if ($Release) { "release" } else { "debug" }
    & $cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }
}
finally {
    Pop-Location
}

if ($Install) {
    & (Join-Path $PSScriptRoot "install.ps1") -Release:$Release -GamePath $GamePath
}
