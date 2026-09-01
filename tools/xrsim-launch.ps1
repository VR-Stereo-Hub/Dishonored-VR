# xrsim-launch.ps1 - launch Dishonored against the SIMULATED OpenXR runtime.
#
# A separate launcher rather than a switch on launch-game.ps1, because it must
# start the exe DIRECTLY: XR_RUNTIME_JSON is inherited from the parent process,
# and Steam launches the game itself, so going through Steam would mean setting
# the variable machine-wide - exactly what this design avoids. The guards are
# shared by CALLING launch-game.ps1 -PreflightOnly.
#
# Two env vars go to the game: XR_RUNTIME_JSON (the sim manifest, read by the
# mod's own loader negotiation before the registry) and DISHONORED_VR_BACKEND=
# openxr (the mod's AUTO backend choice would pick OpenVR or nothing with no
# headset software running).
#
# The single most valuable check here is the runtime-name assertion. If
# XR_RUNTIME_JSON silently fails to take (an elevated shell, a bad manifest
# path, a 64-bit dll), the mod falls through to the registry runtime, nothing
# renders, and every later result is measured against the wrong runtime while
# the transcript claims otherwise. This throws instead.
#
# Usage:
#   $g = .\tools\xrsim-launch.ps1
#   .\tools\xrsim-launch.ps1 -AllowStale -WaitSeconds 120
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
[CmdletBinding()]
param(
    [string]$GamePath = "",
    [switch]$Release,
    [switch]$AllowStale,
    [switch]$NoInstall,
    [switch]$NoWaitSession,
    [int]$WaitSeconds = 90,
    [string]$Dir = "",
    [string[]]$ExtraArgs = @()
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")

$repo   = Split-Path -Parent $PSScriptRoot
$config = if ($Release) { "RelWithDebInfo" } else { "Debug" }
if (-not $Dir) { $Dir = Join-Path (Get-DvrDataDir) "xrsim" }
$GamePath = Get-DvrGamePath $GamePath
$exe = Join-Path $GamePath "Dishonored.exe"
$modLog = Join-Path $GamePath "dishonored_vr.log"

# --- guard 0: elevation ------------------------------------------------------
# The OpenXR loader reads XR_RUNTIME_JSON through a SECURE env path that returns
# nothing for a high-integrity process. The mod's own negotiation reads the
# plain environment, but keep the rule: an elevated shell is the wrong shell.
$id = [Security.Principal.WindowsIdentity]::GetCurrent()
if ((New-Object Security.Principal.WindowsPrincipal($id)).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "REFUSING: this shell is elevated. Use a normal PowerShell window."
}

# --- guards 1 and 2: shared with the normal launcher -------------------------
& (Join-Path $PSScriptRoot "launch-game.ps1") -GamePath $GamePath -PreflightOnly | Out-Null

# --- guard 3: the installed mod is older than what you just built -------------
$builtDll = Join-Path $repo "build\src\$config\d3d9.dll"
$instDll  = Join-Path $GamePath "d3d9.dll"
if (-not $AllowStale -and (Test-Path $builtDll) -and (Test-Path $instDll)) {
    if ((Get-Item $instDll).LastWriteTimeUtc -lt (Get-Item $builtDll).LastWriteTimeUtc) {
        throw "the INSTALLED d3d9.dll is older than your build - run .\tools\install.ps1 first (or pass -AllowStale)."
    }
}

if (-not $NoInstall) {
    & (Join-Path $PSScriptRoot "xrsim-install.ps1") -Release:$Release -Dir $Dir | Out-Null
}
$manifest = Join-Path $Dir "dvr_xrsim32.json"
if (-not (Test-Path $manifest)) { throw "no manifest at $manifest - run xrsim-install.ps1." }

# --- launch ------------------------------------------------------------------
$savedRuntime = $env:XR_RUNTIME_JSON
$savedDir     = $env:DVR_XRSIM_DIR
$savedBackend = $env:DISHONORED_VR_BACKEND
try {
    $env:XR_RUNTIME_JSON = $manifest
    $env:DVR_XRSIM_DIR   = $Dir
    $env:DISHONORED_VR_BACKEND = "openxr"
    Write-Host "launching Dishonored.exe directly with XR_RUNTIME_JSON -> the simulator (backend forced to openxr)"
    $p = if ($ExtraArgs -and $ExtraArgs.Count -gt 0) {
        Start-Process -FilePath $exe -WorkingDirectory $GamePath -ArgumentList $ExtraArgs -PassThru
    } else {
        Start-Process -FilePath $exe -WorkingDirectory $GamePath -PassThru
    }
} finally {
    $env:XR_RUNTIME_JSON = $savedRuntime
    $env:DVR_XRSIM_DIR   = $savedDir
    $env:DISHONORED_VR_BACKEND = $savedBackend
}

Start-Sleep -Seconds 5
if ($p.HasExited) {
    throw "the game exited within 5 s of a direct launch (exit $($p.ExitCode)). " +
          "Is the Steam client running? Start Steam, but do NOT launch the game from it."
}

if ($NoWaitSession) {
    return [pscustomobject]@{ Pid = $p.Id; Exe = $exe; Log = $modLog; Dir = $Dir }
}

# --- wait for the mod to report a live session -------------------------------
# The mod logs: xr: runtime "dvr-xrsim"   then   xr: pipeline READY
$runtimeName = $null
$sessionUp   = $false
for ($i = 0; $i -lt $WaitSeconds; $i++) {
    if (Test-Path $modLog) {
        $lines = Get-Content $modLog -ErrorAction SilentlyContinue
        foreach ($l in $lines) {
            if ($l -match 'xr: runtime "([^"]+)"') { $runtimeName = $Matches[1] }
            if ($l -match "xr: pipeline READY") { $sessionUp = $true }
        }
        if ($runtimeName -and $runtimeName -ne "dvr-xrsim") {
            throw "the simulator was NOT picked up - the mod is on runtime '$runtimeName'. " +
                  "XR_RUNTIME_JSON did not take: elevated shell, a bad manifest path, or a 64-bit dll."
        }
        if ($sessionUp) { break }
    }
    Start-Sleep -Seconds 1
}

if (-not $runtimeName) { throw "the mod never logged an XR runtime within $WaitSeconds s (log: $modLog)." }
if (-not $sessionUp) {
    throw "runtime '$runtimeName' loaded but the pipeline never reported READY within $WaitSeconds s. " +
          "Check the log for 'xr:' bring-up refusals (no HMD, bad view config, swapchain)."
}

# --- confirm frames are actually advancing -----------------------------------
$statePath = Join-Path $Dir "state.json"
function Read-State {
    for ($k = 0; $k -lt 5; $k++) {
        try { return Get-Content $statePath -Raw -ErrorAction Stop | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 120 }
    }
    throw "could not read $statePath"
}
$s1 = Read-State
Start-Sleep -Seconds 1
$s2 = Read-State
if ($s2.frame -le $s1.frame) {
    throw "the simulator is loaded but produced no frames in 1 s (frame stuck at $($s1.frame))."
}

Write-Host "runtime '$runtimeName', session $($s2.sessionState), frame $($s2.frame) (+$($s2.frame - $s1.frame)/s)"

[pscustomobject]@{
    Pid          = $p.Id
    Exe          = $exe
    Log          = $modLog
    Dir          = $Dir
    Runtime      = $runtimeName
    SessionState = $s2.sessionState
    Frame        = $s2.frame
}
