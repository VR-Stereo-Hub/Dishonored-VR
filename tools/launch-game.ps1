# launch-game.ps1 - launch Dishonored through Steam, with the checks that keep a
# test run honest. USE THIS instead of a bare Start-Process steam://.
#
# A check that only PRINTS is not a check: every guard here throws.
#   1. Dishonored already running (a sim launch must start the process itself,
#      because XR_RUNTIME_JSON is per-process and cannot be applied afterwards).
#   2. A stale command.txt. The mod's poller re-applies it at boot because its
#      cached write time starts zeroed - so last session's `fov 130` silently
#      returns. Cleared automatically.
#
# Usage:
#   .\tools\launch-game.ps1
#   .\tools\launch-game.ps1 -PreflightOnly     # guards only (xrsim-launch.ps1 calls this)
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
[CmdletBinding()]
param(
    [switch]$PreflightOnly,
    [int]$WaitSeconds = 60,
    [string]$GamePath = ""
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$appId = $script:DvrAppId
$proc = "Dishonored"

# --- guard 1: already running ------------------------------------------------
if (Get-Process $proc -ErrorAction SilentlyContinue) {
    if ($PreflightOnly) {
        throw "REFUSING: $proc is already running, on whatever runtime it was started with. " +
              "A sim launch has to start the process itself."
    }
    Write-Output "$proc is already running - nothing to do."
    return
}

# --- guard 2: stale command file ---------------------------------------------
$cmd = Get-DvrCmdPath
if (Test-Path $cmd) {
    $stale = (Get-Content $cmd -Raw).Trim()
    Remove-Item $cmd -Force
    if ($stale) { Write-Output "cleared a stale command.txt (would have re-applied at boot): $stale" }
}

if ($PreflightOnly) {
    Write-Output "preflight ok - guards passed, command.txt clear, nothing launched"
    return
}

$gameDir = Get-DvrGamePath $GamePath
Write-Output "launching Dishonored (appid $appId) from $gameDir ..."
Start-Process "steam://rungameid/$appId"

for ($i = 0; $i -lt $WaitSeconds; $i++) {
    Start-Sleep -Seconds 1
    $p = Get-Process $proc -ErrorAction SilentlyContinue
    if ($p -and $p.MainWindowHandle -ne 0) {
        Write-Output "$proc up (pid $($p.Id)). Log: $(Join-Path $gameDir 'dishonored_vr.log')"
        return
    }
}
throw "$proc did not appear within $WaitSeconds s."
