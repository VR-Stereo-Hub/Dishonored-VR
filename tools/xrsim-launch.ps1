# xrsim-launch.ps1 - launch Dishonored against the SIMULATED OpenXR runtime.
#
# A separate launcher rather than a switch on launch-game.ps1, because it must
# start the exe DIRECTLY: XR_RUNTIME_JSON is inherited from the parent process,
# and Steam launches the game itself, so going through Steam would mean setting
# the variable machine-wide - exactly what this design avoids. The guards are
# shared by CALLING launch-game.ps1 -PreflightOnly.
#
# Two env vars go to the game: XR_RUNTIME_JSON (the sim manifest, read first by
# the static OpenXR loader inside d3d9.dll) and DVR_XRSIM_DIR (where the sim
# keeps state.json and captures). 41.0 is OpenXR-only, so no backend needs
# forcing any more.
#
# -ViaSteam: the original author's handoff says a direct exe launch crashes at
# the menu on some setups (docs/dishonored/HANDOFF-GINGASVR.md, trap 6; it did
# NOT reproduce here in session 4). Steam starts the exe itself, so the env var
# cannot reach it: -ViaSteam writes the manifest into dishonored_vr.ini as
# [VR] XrRuntimeJson (the mod sets XR_RUNTIME_JSON for its own process from it
# when the environment carries none), launches through launch-game.ps1, and
# restores the ini once the runtime-name assertion has passed (or on failure).
# DVR_XRSIM_DIR cannot reach the game either, so -Dir must stay the default.
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
    [switch]$ViaSteam,
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

# --- the ini key for a Steam launch -------------------------------------------
# Sets one key in one section of an ini (creates the section if absent), ASCII
# in, ASCII out. The mod reads the file with GetPrivateProfileString, which
# tolerates CRLF or LF.
function Set-IniKey([string]$path, [string]$section, [string]$key, [string]$value) {
    $lines = @()
    if (Test-Path $path) { $lines = @(Get-Content $path) }
    $out = New-Object System.Collections.Generic.List[string]
    $inSection = $false; $done = $false; $sawSection = $false
    foreach ($l in $lines) {
        if ($l -match '^\s*\[(.+)\]\s*$') {
            if ($inSection -and -not $done) { $out.Add("$key=$value"); $done = $true }
            $inSection = ($Matches[1] -eq $section)
            if ($inSection) { $sawSection = $true }
            $out.Add($l); continue
        }
        if ($inSection -and -not $done -and $l -match ('^\s*' + [regex]::Escape($key) + '\s*=')) {
            $out.Add("$key=$value"); $done = $true; continue
        }
        $out.Add($l)
    }
    if (-not $done) {
        if (-not $sawSection) { $out.Add("[$section]") }
        $out.Add("$key=$value")
    }
    [System.IO.File]::WriteAllLines($path, $out.ToArray(), (New-Object System.Text.ASCIIEncoding))
}

# --- launch ------------------------------------------------------------------
$iniPath = Join-Path $GamePath "dishonored_vr.ini"
$iniBackup = $null
$script:iniRestored = $false
function Restore-Ini {
    if ($script:iniRestored) { return }
    $script:iniRestored = $true
    if ($iniBackup -and (Test-Path $iniBackup)) {
        Move-Item $iniBackup $iniPath -Force
        Write-Host "restored $iniPath from the pre-launch backup"
    } elseif ($ViaSteam -and (Test-Path $iniPath) -and -not $iniBackup) {
        Set-IniKey $iniPath "VR" "XrRuntimeJson" ""
    }
}
$savedRuntime = $env:XR_RUNTIME_JSON
$savedDir     = $env:DVR_XRSIM_DIR
$p = $null
try {
    $env:XR_RUNTIME_JSON = $manifest
    $env:DVR_XRSIM_DIR   = $Dir
    if ($ViaSteam) {
        if ($Dir -ne (Join-Path (Get-DvrDataDir) "xrsim")) {
            throw "-ViaSteam cannot pass -Dir to the game (Steam starts the exe); use the default xrsim dir."
        }
        if (Test-Path $iniPath) { $iniBackup = "$iniPath.xrsim-bak"; Copy-Item $iniPath $iniBackup -Force }
        Set-IniKey $iniPath "VR" "XrRuntimeJson" $manifest
        Write-Host "launching through Steam with [VR] XrRuntimeJson -> the simulator (ini backed up)"
        & (Join-Path $PSScriptRoot "launch-game.ps1") -GamePath $GamePath -WaitSeconds 150 | Out-Null
    } else {
        Write-Host "launching Dishonored.exe directly with XR_RUNTIME_JSON -> the simulator"
        $p = if ($ExtraArgs -and $ExtraArgs.Count -gt 0) {
            Start-Process -FilePath $exe -WorkingDirectory $GamePath -ArgumentList $ExtraArgs -PassThru
        } else {
            Start-Process -FilePath $exe -WorkingDirectory $GamePath -PassThru
        }
    }
} finally {
    $env:XR_RUNTIME_JSON = $savedRuntime
    $env:DVR_XRSIM_DIR   = $savedDir
}

Start-Sleep -Seconds 5
if ($ViaSteam) {
    for ($k = 0; $k -lt 25 -and -not $p; $k++) {
        $p = Get-Process -Name Dishonored -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $p) { Start-Sleep -Seconds 1 }
    }
    if (-not $p) { Restore-Ini; throw "Steam did not start Dishonored.exe within 30 s." }
} elseif ($p.HasExited) {
    throw "the game exited within 5 s of a direct launch (exit $($p.ExitCode)). " +
          "Is the Steam client running? Start Steam, but do NOT launch the game from it " +
          "(or use -ViaSteam, which launches through Steam with the manifest in the ini)."
}

if ($NoWaitSession) {
    if ($ViaSteam) { Write-Host "NOTE: -NoWaitSession with -ViaSteam leaves [VR] XrRuntimeJson in the ini (backup: $iniBackup)" }
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
            Restore-Ini
            throw "the simulator was NOT picked up - the mod is on runtime '$runtimeName'. " +
                  "XR_RUNTIME_JSON did not take: elevated shell, a bad manifest path, or a 64-bit dll."
        }
        if ($runtimeName) { Restore-Ini }   # the mod has read the ini by now
        if ($sessionUp) { break }
    }
    Start-Sleep -Seconds 1
}

Restore-Ini
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
