# Builds the mod (32-bit): d3d9.dll (the proxy + VR core), the simulated OpenXR
# runtime, the dvr_steamvr32 SteamVR shim and the xr_hello32 smoke client. CMake
# is not on PATH on this machine;
# we use the one bundled with VS 2022 Build Tools, located via vswhere.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [switch]$Release,
    [switch]$Install,
    [switch]$FlickerDiagnostics, # VR-229 test ZIP only; never installed automatically
    [switch]$Legacy,      # also compile src/legacy (retired experiments)
    [string]$GamePath = ""
)

$ErrorActionPreference = "Stop"
if ($FlickerDiagnostics -and $Install) { throw "FlickerDiagnostics is ZIP-only; build without -Install." }
$repo = Split-Path -Parent $PSScriptRoot

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install VS 2022 Build Tools." }
$cmake = & $vswhere -latest -products * -find "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" | Select-Object -First 1
if (-not $cmake) { throw "VS-bundled CMake not found - install the 'C++ CMake tools' component." }

# cmake presets are resolved relative to the current directory
Push-Location $repo
try {
    # VR-180: the legacy switch is decided by THIS call and by nothing else.
    # It used to be passed only on a first configure or with -Legacy, so a build
    # directory that had EVER seen -Legacy kept DVR_WITH_LEGACY=ON in its CMake
    # cache, and every later plain build quietly compiled the retired diagnostics
    # in. One of them scans the engine's whole object table on every trigger pull:
    # that build was installed for a headset session and froze the game for a
    # quarter second per pull. Read what the cache holds, and reconfigure whenever
    # it is not what was asked for.
    $diagnosticFlag = if ($FlickerDiagnostics) { "ON" } else { "OFF" }
    $legacyFlag = if ($Legacy) { "ON" } else { "OFF" }
    if (-not (Test-Path "build\CMakeCache.txt")) {
        & $cmake --preset win32 "-DDVR_WITH_LEGACY=$legacyFlag" "-DDVR_FLICKER_DIAGNOSTICS=$diagnosticFlag"
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
    } else {
        $cached = (Select-String -Path "build\CMakeCache.txt" -Pattern '^DVR_WITH_LEGACY:BOOL=(\w+)' |
                   Select-Object -First 1)
        $cachedFlag = if ($cached) { $cached.Matches[0].Groups[1].Value.ToUpper() } else { "" }
        $cachedDiagnostic = Select-String -Path "build\CMakeCache.txt" -Pattern "^DVR_FLICKER_DIAGNOSTICS:BOOL=$diagnosticFlag$"
        if ($cachedFlag -ne $legacyFlag -or -not $cachedDiagnostic) {
            Write-Host "build: the CMake cache holds DVR_WITH_LEGACY=$cachedFlag and this build asks for $legacyFlag - reconfiguring" -ForegroundColor Yellow
            & $cmake -S . -B build "-DDVR_WITH_LEGACY=$legacyFlag" "-DDVR_FLICKER_DIAGNOSTICS=$diagnosticFlag"
            if ($LASTEXITCODE -ne 0) { throw "CMake reconfigure failed." }
        }
    }
    if ($Legacy) {
        Write-Host "build: LEGACY code is compiled IN (-Legacy). For the debugger and old diagnostics only - never for playing or for a tester." -ForegroundColor Yellow
    } else {
        Write-Host "build: legacy code is compiled out (the default)."
    }

    $preset = if ($Release) { "release" } else { "debug" }
    & $cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }
    $cfg = if ($Release) { "RelWithDebInfo" } else { "Debug" }
    $versionText = Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
    if ($versionText -notmatch 'project\(DishonoredVR VERSION ([0-9.]+)') { throw 'Cannot read launcher version' }
    $version = $Matches[1]
    $setup = Join-Path $repo "build\src\$cfg\DishonoredVR-Launcher-v$version.exe"
    if (Test-Path $setup) { Write-Host "build: installer $setup (embeds this build's d3d9.dll; tools\installer-render.ps1 draws its screens)" }
}
finally {
    Pop-Location
}

if ($Install) {
    & (Join-Path $PSScriptRoot "install.ps1") -Release:$Release -GamePath $GamePath
}
