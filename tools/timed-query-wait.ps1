# Timed engine query-wait diagnostic A/B/A. Never launches the game or starts a recorder.
# Commands are live only; the installed launch-arm INI remains unchanged.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$Out,
 [Parameter(Mandatory)][string]$GameLog,
 [Parameter(Mandatory)][string]$ExpectedBuild,
 [Parameter(Mandatory)][string]$DataDir
)
$ErrorActionPreference='Stop'
$Out=[IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force $Out | Out-Null
$changed=$false
function State([string]$s) {
 [IO.File]::WriteAllText((Join-Path $Out 'state.json'),(@{state=$s;utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json))
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
function Command([string]$s) {
 # Do not steal foreground focus. A pending unrelated command is an error.
 $path=Join-Path $DataDir 'command.txt'
 if ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).Length -gt 0) {
  throw 'An unrelated command is pending; leaving it untouched.'
 }
 [IO.File]::WriteAllText($path,"$s`n")
 $until=[DateTime]::UtcNow.AddSeconds(5)
 while ((Get-Item -LiteralPath $path).Length -gt 0) {
  if ([DateTime]::UtcNow -ge $until) { throw "Command was not consumed: $s" }
  Start-Sleep -Milliseconds 100
 }
 State $s
}
function PauseGame([int]$seconds) {
 $until=[DateTime]::UtcNow.AddSeconds($seconds)
 while ([DateTime]::UtcNow -lt $until) {
  if ($game.HasExited) { throw 'Game exited early; phases are incomplete.' }
  Start-Sleep -Milliseconds 500
 }
}
try {
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
 Command 'querywait off'
 Command 'mark query_baseline_before'
 PauseGame 30
 $changed=$true
 Command 'querywait on'
 Command 'mark query_profile_start'
 PauseGame 40
 Command 'querywait off'
 $changed=$false
 Command 'mark query_baseline_after'
 PauseGame 30
 Command 'mark query_test_complete'
 Copy-Item -LiteralPath $GameLog -Destination (Join-Path $Out 'dishonored_vr.log')
 State 'complete'
} catch {
 $_ | Out-String | Set-Content -LiteralPath (Join-Path $Out 'error.txt')
 State 'failed'
} finally {
 if ($changed -and $game -and !$game.HasExited) {
  try { Command 'querywait off' } catch {
   $_ | Out-String | Add-Content -LiteralPath (Join-Path $Out 'error.txt')
  }
 }
}
