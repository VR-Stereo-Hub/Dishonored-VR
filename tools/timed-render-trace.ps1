# Timed real-headset profiling. This script never launches the game.
# Run elevated. Artifacts stay local; no configuration or engine-memory writes.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$Out,
 [Parameter(Mandatory)][string]$GameLog,
 [Parameter(Mandatory)][string]$ExpectedBuild,
 [Parameter(Mandatory)][string]$DataDir,
 [switch]$SmokeOnly
)
$ErrorActionPreference='Stop'
$profile=Join-Path $PSScriptRoot 'wpr\dvr-render.wprp'
$Out=[IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force $Out | Out-Null
$owned=$false
function State([string]$s) {
 [IO.File]::WriteAllText((Join-Path $Out 'state.json'),(@{state=$s;utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json))
}
function RecordStart {
 $status=& wpr.exe -status 2>&1 | Out-String
 $status | Set-Content -LiteralPath (Join-Path $Out 'before-start.txt')
 if ($LASTEXITCODE -ne 0 -or $status -notmatch 'WPR is not recording') {
  throw 'Recorder is not confirmed idle; leaving existing sessions untouched.'
 }
 & wpr.exe -start "$profile!DvrRender" *> (Join-Path $Out 'start.txt')
 if ($LASTEXITCODE -ne 0) { throw 'WPR start failed; no existing recording was cancelled.' }
 $script:owned=$true
}
function RecordStop([string]$name) {
 & wpr.exe -status collectors *> (Join-Path $Out "$name-status.txt")
 & wpr.exe -stop (Join-Path $Out "$name.etl") *> (Join-Path $Out "$name-stop.txt")
 if ($LASTEXITCODE -ne 0) { throw 'WPR save failed; inspect stop output.' }
 $script:owned=$false
}
function TailLog {
 if (!(Test-Path -LiteralPath $GameLog)) { return '' }
 $fs=[IO.File]::Open($GameLog,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 try {
  $n=[int][Math]::Min(65536,$fs.Length)
  [void]$fs.Seek(-$n,[IO.SeekOrigin]::End)
  $b=New-Object byte[] $n
  $read=$fs.Read($b,0,$n)
  return [Text.Encoding]::UTF8.GetString($b,0,$read)
 } finally { $fs.Dispose() }
}
function Mark([string]$s) {
 # Do not steal foreground focus. A pending unrelated command is an error.
 $path=Join-Path $DataDir 'command.txt'
 if ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).Length -gt 0) {
  throw 'An unrelated command is pending; leaving it untouched.'
 }
 [IO.File]::WriteAllText($path,"mark $s`n")
 State $s
}
function PauseGame([int]$seconds) {
 $until=[DateTime]::UtcNow.AddSeconds($seconds)
 while ([DateTime]::UtcNow -lt $until) {
  if ($game.HasExited) { throw 'Game exited early; capture is saved but phases are incomplete.' }
  Start-Sleep -Milliseconds 500
 }
}
try {
 $admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
 if (!$admin) { throw 'Windows administrator token required.' }
 if ($SmokeOnly) {
  RecordStart
  Start-Sleep -Seconds 2
  RecordStop 'smoke'
  State 'smoke_saved'
  exit 0
 }
 if (Get-Process Dishonored -ErrorAction SilentlyContinue) { throw 'Arm before the tester launches the game.' }
 State 'armed_waiting_for_game'
 $deadline=[DateTime]::UtcNow.AddMinutes(15)
 $game=$null
 while ([DateTime]::UtcNow -lt $deadline) {
  $game=Get-Process Dishonored -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($game) { break }
  Start-Sleep -Seconds 1
 }
 if (!$game) { throw 'No game launched within 15 minutes.' }
 State 'waiting_for_hub'
 $matched=$false
 while ([DateTime]::UtcNow -lt $deadline) {
  if ($game.HasExited) { throw 'Game exited before hub trigger.' }
  if (!$matched -and (Test-Path -LiteralPath $GameLog) -and
      (Get-Item -LiteralPath $GameLog).LastWriteTimeUtc -ge $game.StartTime.ToUniversalTime()) {
   $first=Get-Content -LiteralPath $GameLog -TotalCount 1
   $matched=$first -like "*build $ExpectedBuild,*"
  }
  if ($matched -and (TailLog) -match 'perf: tick[^\r\n]+SRT (?:[7-9][0-9]|[1-9][0-9]{2,})\.[0-9]+/present') { break }
  Start-Sleep -Seconds 1
 }
 if (!$matched -or [DateTime]::UtcNow -ge $deadline) { throw 'Expected build and populated gameplay log were not established.' }
 Mark 'etw_baseline_start'
 PauseGame 30
 RecordStart
 Mark 'etw_capture_start'
 PauseGame 40
 Mark 'etw_capture_end'
 RecordStop 'headset'
 # Saving/rundown is excluded from the final baseline.
 Mark 'etw_cooldown_start'
 PauseGame 10
 Mark 'etw_baseline_after_start'
 PauseGame 30
 Mark 'etw_test_complete'
 Copy-Item -LiteralPath $GameLog -Destination (Join-Path $Out 'dishonored_vr.log')
 State 'complete'
} catch {
 $_ | Out-String | Set-Content -LiteralPath (Join-Path $Out 'error.txt')
 State 'failed'
} finally {
 if ($owned) {
  try { RecordStop 'early-stop' } catch {
   $_ | Out-String | Add-Content -LiteralPath (Join-Path $Out 'error.txt')
  }
 }
}
