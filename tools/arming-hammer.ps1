# arming-hammer.ps1 - hammer the pause/resume path under `stereo reentry` and
# FAIL, with an exit code, if any eye went stale.
#
# The 2026-09-03 headset run saw the two eyes desync occasionally after a
# pause/resume: the right eye stopped taking fresh frames. It never showed on
# the simulator's single clean transition (run 38), so this loops the
# transition with seeded, varied dwell times and reads the two instruments
# that can print the unwelcome answer: the sim's per-eye release age at each
# projection submit (state.json eyeAgeL/R, projStaleSubmits) and the mod's own
# pair probe (status.json stereo.pair.staleL/R, the STALE .. EYE log line).
#
# Usage (the game already in GAMEPLAY on the simulator, window reachable):
#   .\tools\arming-hammer.ps1 -Cycles 30 -Seed 1 -Dir D:\dvr-data\xrsim
#
# Exit codes: 0 pass; 2 the sim counted stale submits; 3 the mod's pair probe
# counted stale eyes or logged a STALE EYE line; 5 not in GAMEPLAY / not FOCUSED.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
[CmdletBinding()]
param(
    [int]$Cycles = 30,
    [int]$Seed = 1,
    [int]$MinDelayMs = 100,
    [int]$MaxDelayMs = 1500,
    [string]$Dir = "$env:LOCALAPPDATA\DishonoredVR\xrsim",
    [string]$GamePath = ""
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$stateScript = Join-Path $PSScriptRoot "xrsim-state.ps1"
$keyScript   = Join-Path $PSScriptRoot "game-key.ps1"
$gameCmd     = Join-Path $PSScriptRoot "game-cmd.ps1"

function Read-ModStatus {
    $p = Get-DvrStatusPath
    if (-not (Test-Path $p)) { return $null }
    try { return Get-Content $p -Raw | ConvertFrom-Json } catch { return $null }
}

$rand = New-Object System.Random($Seed)
$s0 = & $stateScript -Dir $Dir -Quiet
if ($s0.sessionState -ne 'FOCUSED') { Write-Host "FAIL: sim session is $($s0.sessionState), not FOCUSED"; exit 5 }
$m0 = Read-ModStatus
if (-not $m0 -or $m0.state -ne 'GAMEPLAY') {
    Write-Host "FAIL: the mod reports state '$(if ($m0) { $m0.state } else { 'no status.json' })', not GAMEPLAY - walk into a level first"
    exit 5
}
& $gameCmd "stereo reentry" | Out-Null
Start-Sleep -Milliseconds 1500
$s0 = & $stateScript -Dir $Dir -Quiet
if ($s0.projectionViews -ne 2) { Write-Host "FAIL: no projection layer after 'stereo reentry' (projectionViews=$($s0.projectionViews))"; exit 5 }
$log = Get-DvrLogPath $GamePath
$logLines0 = if (Test-Path $log) { (Get-Content $log | Measure-Object -Line).Lines } else { 0 }

$staleStart = [int]$s0.projStaleSubmits
$monoStart  = [int]$s0.projMonoSubmits
$worstAge   = 0
$staleCycles = 0
Write-Host "hammer: $Cycles pause/resume cycles, seed $Seed, dwell $MinDelayMs..$MaxDelayMs ms; sim stale=$staleStart mono=$monoStart at start"
for ($i = 1; $i -le $Cycles; $i++) {
    $dwell  = $rand.Next($MinDelayMs, $MaxDelayMs + 1)
    $settle = $rand.Next($MinDelayMs, $MaxDelayMs + 1)
    $before = & $stateScript -Dir $Dir -Quiet
    & $keyScript -Key Escape | Out-Null            # pause
    Start-Sleep -Milliseconds $dwell
    & $keyScript -Key Escape | Out-Null            # resume
    Start-Sleep -Milliseconds $settle
    & $stateScript -Dir $Dir -For "frame+60" -TimeoutSec 20 -Quiet | Out-Null
    $after = & $stateScript -Dir $Dir -Quiet
    $dStale = [int]$after.projStaleSubmits - [int]$before.projStaleSubmits
    $ageL = [int]$after.eyeAgeL; $ageR = [int]$after.eyeAgeR
    if ($ageL -gt $worstAge) { $worstAge = $ageL }
    if ($ageR -gt $worstAge) { $worstAge = $ageR }
    if ($dStale -gt 0) { $staleCycles++ }
    Write-Host ("[{0,2}/{1}] dwell {2,4} ms settle {3,4} ms | proj {4} age L/R {5}/{6} | stale submits +{7} (mono +{8})" -f `
        $i, $Cycles, $dwell, $settle, $after.projectionViews, $ageL, $ageR, $dStale,
        ([int]$after.projMonoSubmits - [int]$before.projMonoSubmits))
}

$sEnd = & $stateScript -Dir $Dir -Quiet
$simStale = [int]$sEnd.projStaleSubmits - $staleStart
& $gameCmd -NoFocus "status" | Out-Null
Start-Sleep -Milliseconds 1500
$m = Read-ModStatus
$modStaleL = 0; $modStaleR = 0
if ($m -and $m.stereo -and $m.stereo.pair) { $modStaleL = [int]$m.stereo.pair.staleL; $modStaleR = [int]$m.stereo.pair.staleR }
$staleLines = 0
if (Test-Path $log) {
    $staleLines = @(Get-Content $log | Select-Object -Skip $logLines0 | Select-String -Pattern "STALE . EYE").Count
}
Write-Host "hammer: done - sim stale submits $simStale over $Cycles cycles ($staleCycles cycles with any), worst eye age $worstAge frames | mod pair staleL=$modStaleL staleR=$modStaleR, STALE EYE lines this run: $staleLines"
if ($simStale -gt 0) { Write-Host "FAIL: the sim saw a held eye in a stereo submit"; exit 2 }
if ($modStaleL -gt 0 -or $modStaleR -gt 0 -or $staleLines -gt 0) { Write-Host "FAIL: the mod's pair probe counted a stale eye (read the STALE .. EYE line for the owner)"; exit 3 }
Write-Host "PASS: no stale eye in $Cycles pause/resume cycles"
exit 0
