# Copies the built mod into the game folder (Binaries\Win32 next to
# Dishonored.exe): d3d9.dll (the proxy), dvr_steamvr32.dll (the SteamVR shim
# runtime) and openvr_api.dll (Valve's 32-bit loader, vendored, hash-checked).
# Backs up a pre-existing d3d9.dll that is not ours as d3d9.dll.dvr-backup once.
# Deliberately NOT guarded against a running game or headset: copying files
# touches the disk only (the copy fails if the DLL is loaded, which is fine).
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [switch]$Release,
    [switch]$AllowLegacy,   # VR-180: install an optimised build that carries src/legacy anyway
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

# VR-180: an OPTIMISED build is one somebody is about to play. Refuse it BEFORE anything
# is copied if it carries the legacy code, unless that is said out loud. A Debug build
# is installed and warned about below.
$legacyBuild = Test-DvrLegacyDll $proxy
if ($legacyBuild -and $Release -and -not $AllowLegacy) {
    throw ("REFUSING to install: $proxy has the legacy code compiled in, and an optimised build is a build " +
           "somebody plays. Rebuild with a plain  tools\build.ps1 -Release  (it now reconfigures the legacy " +
           "switch off), or pass -AllowLegacy if a legacy diagnostic in the headset is really what you want.")
}

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

$bridge = Join-Path $outDir "DishonoredVR_ReShade.addon32"
if (-not (Test-Path -LiteralPath $bridge)) { throw "Missing ReShade bridge; rebuild this candidate." }
Assert-DvrX86Dll $bridge
Copy-Item $proxy $existing -Force
Copy-Item -LiteralPath $bridge -Destination (Join-Path $GamePath "DishonoredVR_ReShade.addon32") -Force

# The SteamVR shim runtime and Valve's 32-bit loader (hash-checked against
# third_party\openvr_headers\PROVENANCE.txt). SteamVR rigs need both; VDXR and
# other native 32-bit runtimes never load them.
$shim = Join-Path $outDir "dvr_steamvr32.dll"
if (Test-Path $shim) {
    Assert-DvrX86Dll $shim
    $ovr = Join-Path $repo "third_party\openvr_headers\bin\win32\openvr_api.dll"
    $prov = Get-Content (Join-Path $repo "third_party\openvr_headers\PROVENANCE.txt") -Raw
    $hash = (Get-FileHash $ovr -Algorithm SHA256).Hash.ToLower()
    if ($prov -notmatch [regex]::Escape($hash)) { throw "openvr_api.dll sha256 $hash is not in PROVENANCE.txt - refusing to install" }
    Copy-Item $shim (Join-Path $GamePath "dvr_steamvr32.dll") -Force
    Copy-Item $ovr (Join-Path $GamePath "openvr_api.dll") -Force
} else {
    Write-Host "dvr_steamvr32.dll not built (DVR_WITH_OVRSHIM=OFF?) - SteamVR rigs need it"
}
# DLAA (core/gfx/dlss.h): the x64 NGX helper and NVIDIA's runtime in <game>\dvr_dlss\, with
# NVIDIA's license beside them. Optional: without it [Clarity] DLAA=1 logs why and the normal
# path runs. The runtime is hash-checked against the pin in tools\fetch-ngx.ps1.
$dlssHost = Join-Path $repo "build\dlss_host\dvr_dlss_host64.exe"
$dlssDll = Join-Path $repo "build\dlss_host\nvngx_dlss.dll"
$dlssInstalled = $false
if ((Test-Path $dlssHost) -and (Test-Path $dlssDll)) {
    $pin = "BE6E434A94CA32499515EB62CA0E6C274526055D568D0426E4C652DCDFB6EE6E"
    $got = (Get-FileHash $dlssDll -Algorithm SHA256).Hash
    if ($got -ne $pin) { throw "nvngx_dlss.dll sha256 $got is not the pinned $pin - refusing to install" }
    $dlssDir = Join-Path $GamePath "dvr_dlss"
    New-Item -ItemType Directory -Force -Path $dlssDir | Out-Null
    Copy-Item $dlssHost (Join-Path $dlssDir "dvr_dlss_host64.exe") -Force
    Copy-Item $dlssDll (Join-Path $dlssDir "nvngx_dlss.dll") -Force
    Copy-Item (Join-Path $repo "third_party\ngx\LICENSE.txt") (Join-Path $dlssDir "NVIDIA-DLSS-LICENSE.txt") -Force
    Copy-Item (Join-Path $repo "src\tools\dlss_host\NOTICE.md") (Join-Path $dlssDir "NOTICE.md") -Force
    # FSR: AMD's FidelityFX API loader and upscaler (SDK 2.3.0: FSR 4.1.1 on RDNA 4, FSR 3.1 elsewhere),
    # pinned in tools\fetch-ffx.ps1, and their MIT license. The SDK 1.1.4 DLL an older install left is removed.
    $ffxPins = @{
        "amd_fidelityfx_loader_dx12.dll"   = "E2D85AA05A9BD9ED8B38935FDF5199372CCA6F74C12015143BB6F945EE1608AA"
        "amd_fidelityfx_upscaler_dx12.dll" = "D0DCCCC74A43C44BA435B7A369B456E0970D8A4464E4BD683119B374F2C9FB46"
    }
    $ffxAll = $true
    foreach ($n in $ffxPins.Keys) { if (-not (Test-Path (Join-Path $repo "build\dlss_host\$n"))) { $ffxAll = $false } }
    if ($ffxAll) {
        foreach ($n in $ffxPins.Keys) {
            $f = Join-Path $repo "build\dlss_host\$n"
            $got = (Get-FileHash $f -Algorithm SHA256).Hash
            if ($got -ne $ffxPins[$n]) { throw "$n sha256 $got is not the pinned $($ffxPins[$n]) - refusing to install" }
            Copy-Item $f (Join-Path $dlssDir $n) -Force
        }
        Remove-Item -ErrorAction SilentlyContinue (Join-Path $dlssDir "amd_fidelityfx_dx12.dll")
        Copy-Item (Join-Path $repo "third_party\ffx\LICENSE.md") (Join-Path $dlssDir "AMD-FIDELITYFX-LICENSE.md") -Force
        Remove-Item -ErrorAction SilentlyContinue (Join-Path $dlssDir "AMD-FIDELITYFX-LICENSE.txt")
    }
    $dlssInstalled = $true
} else {
    Write-Host "DLSS helper not built (tools\fetch-ngx.ps1 then tools\build-dlss-host.ps1) - DLAA stays unavailable"
}
# The DXVK fork is gone (41.0). An older install's dxvk_d3d9.dll is loaded by
# nothing any more, but leaving it beside the exe invites confusion.
$oldFork = Join-Path $GamePath "dxvk_d3d9.dll"
if (Test-Path $oldFork) { Remove-Item $oldFork -Force; Write-Host "Removed the retired dxvk_d3d9.dll" }

Write-Host "Installed $config build to $GamePath"
# VR-180: a build carrying the retired diagnostics was installed for a headset session
# and froze the game on every trigger pull, and nothing here said what it was.
if ($legacyBuild) {
    Write-Host ""
    Write-Host "  *** This build has the LEGACY code compiled in (src/legacy, retired diagnostics). ***" -ForegroundColor Red
    Write-Host "  *** One of them scans every engine object on each trigger pull: expect a freeze.  ***" -ForegroundColor Red
    Write-Host "  *** NOT for playing and NEVER for a tester. A plain  build.ps1  builds without it. ***" -ForegroundColor Red
    Write-Host ""
} else {
    Write-Host "  legacy code: not compiled in"
}
Write-Host "  d3d9.dll        $(Get-Item $proxy | Select-Object -ExpandProperty LastWriteTime)  sha256 $((Get-FileHash $proxy -Algorithm SHA256).Hash.Substring(0,16))"
# VR-160: a Debug build was played and measured in the headset for a day because
# this script said "Debug" once, in the middle of four lines. Say it so it is read.
if (-not $Release) {
    Write-Host ""
    Write-Host "  *** This is the UNOPTIMISED Debug build (/Od, runtime checks, the debug CRT). ***" -ForegroundColor Yellow
    Write-Host "  *** Fine for the simulator and the debugger. NOT for playing or for any        ***" -ForegroundColor Yellow
    Write-Host "  *** performance number: use  build.ps1 -Release  then  install.ps1 -Release    ***" -ForegroundColor Yellow
    Write-Host ""
}
if (Test-Path $shim) { Write-Host "  dvr_steamvr32.dll + openvr_api.dll (SteamVR shim, sha256 verified)" }
if ($dlssInstalled) { Write-Host "  dvr_dlss\dvr_dlss_host64.exe + nvngx_dlss.dll 310.7.0.0 + FidelityFX loader 2.3.0 / upscaler 4.1.1 (DLSS/FSR helper, sha256 verified)  sha256 $((Get-FileHash $dlssHost -Algorithm SHA256).Hash.Substring(0,16))" }
Write-Host "Log: $(Join-Path $GamePath 'dishonored_vr.log')   harness files: $(Get-DvrDataDir)"
