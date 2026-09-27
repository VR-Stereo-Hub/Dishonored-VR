# The uncap deep dive's recorder: a GPU + CPU timeline of the frame (WPR, tools\wpr\dvr-gpu.wprp)
# around chosen segments of the A/B plan, plus GPU clocks and per-process engine load for the whole run.
# It NEVER launches the game and never writes the mod's ini. WPR needs admin: run it elevated, or install
# the trace tasks once (tools\perf-trace-task-setup.ps1) and run it as a normal user.
#
#   .\tools\perf-gpu-trace.ps1 -Smoke                 # 4 s of the desktop now: proves the recorder works
#   .\tools\perf-gpu-trace.ps1                        # arm: waits for the game, traces A/B segments
#   .\tools\perf-gpu-trace.ps1 -Segments 'baseline|depth' -Seconds 5 -Profile DvrGpuStacks
#   .\tools\perf-gpu-trace.ps1 -Plan tools\perf-plans\uncap-1.txt   # also ARMS that A/B plan through the seam
#       (copied into the data dir and written as `perf ab plan <name>` once the runtime reports the pipeline
#       READY; the plan itself waits for gameplay, so load the save and stand still)
#
# Output (default <data dir>\perf-trace\<time>): seg<k>-<label>.etl per traced window, trace-windows.csv
# (GetTickCount start/stop of each window, the clock the mod's log uses), nvsmi.csv (clocks, power,
# utilisation, P-state, throttle reasons at 4 Hz), gpu-engines.csv (per-process engine utilisation at
# 1 Hz), dishonored_vr.log (copied at the end). tools\perf-gpu-timeline.py reads the folder.
# Not elevated, it drives WPR through the on-demand tasks tools\perf-trace-task-setup.ps1 installs once.
[CmdletBinding()]
param(
    [string]$Out = "",
    [switch]$Smoke,
    [int]$Seconds = 5,
    [ValidateSet('DvrGpu', 'DvrGpuStacks')][string]$Profile = 'DvrGpu',
    [string]$Segments = '.',            # regex on the A/B segment label; '.' = every segment
    [int]$IntoSegment = 7,              # seconds after a segment starts (its 2 s warm-up is discarded anyway)
    [int]$MaxCaptures = 16,
    [int]$WaitMinutes = 30,
    [string]$Plan = ""                  # an A/B plan file to arm through the seam (no ini edit)
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\game-path.ps1')
$wprp = Join-Path $PSScriptRoot 'wpr\dvr-gpu.wprp'
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
# Not elevated: the on-demand tasks tools\perf-trace-task-setup.ps1 registered run WPR for us.
$taskDir = Join-Path $env:ProgramFiles 'DishonoredVR-Trace'
$viaTask = -not $admin
if ($viaTask -and -not (Test-Path (Join-Path $taskDir 'dvr-wpr-task.ps1'))) {
    throw 'Not elevated and the trace tasks are not installed: run tools\perf-trace-task-setup.ps1 once from an Administrator PowerShell.'
}
if (-not $Out) { $Out = Join-Path (Get-DvrDataDir) ("perf-trace\" + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$Out = [IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force $Out | Out-Null
"perf-gpu-trace: output $Out (WPR $(if ($viaTask) { 'through the \DishonoredVR\ tasks' } else { 'directly, elevated' }))"

$script:recording = $false
function Tick { [uint32]([Environment]::TickCount -band 0x7fffffff) }
function Task-State {
    $f = Join-Path $taskDir 'out\status.txt'
    if (-not (Test-Path $f)) { return 'unknown' }
    return (Get-Content -LiteralPath $f -TotalCount 1)
}
function Run-Task([string]$name) {
    $r = & schtasks.exe /run /tn "\DishonoredVR\$name" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { throw "schtasks /run $name failed: $r" }
}
function Start-Rec {
    if ($viaTask) {
        $was = if (Test-Path (Join-Path $taskDir 'out\status.txt')) { (Get-Item (Join-Path $taskDir 'out\status.txt')).LastWriteTimeUtc } else { [DateTime]::MinValue }
        Run-Task $(if ($Profile -eq 'DvrGpuStacks') { 'WPR-StartStacks' } else { 'WPR-Start' })
        $until = (Get-Date).AddSeconds(20)
        while ((Get-Date) -lt $until) {
            $f = Join-Path $taskDir 'out\status.txt'
            if ((Test-Path $f) -and (Get-Item $f).LastWriteTimeUtc -gt $was) { break }
            Start-Sleep -Milliseconds 200
        }
        if ((Task-State) -ne 'recording') { throw "the WPR-Start task did not start a recording (state $(Task-State); see $taskDir\out\task.log)" }
        $script:recording = $true
        return
    }
    $status = & wpr.exe -status 2>&1 | Out-String
    if ($status -notmatch 'WPR is not recording') { throw "WPR is already recording something else; not touching it:`n$status" }
    & wpr.exe -start "$wprp!$Profile" -filemode *> (Join-Path $Out 'wpr-start.txt')
    if ($LASTEXITCODE -ne 0) { throw "wpr -start failed (see $Out\wpr-start.txt)" }
    $script:recording = $true
}
function Stop-Rec([string]$name) {
    if ($viaTask) {
        $lastFile = Join-Path $taskDir 'out\last.txt'
        $was = if (Test-Path $lastFile) { (Get-Item $lastFile).LastWriteTimeUtc } else { [DateTime]::MinValue }
        Run-Task 'WPR-Stop'
        $script:recording = $false
        $until = (Get-Date).AddSeconds(120)
        while ((Get-Date) -lt $until) {
            if ((Test-Path $lastFile) -and (Get-Item $lastFile).LastWriteTimeUtc -gt $was) { break }
            Start-Sleep -Milliseconds 500
        }
        if (-not ((Test-Path $lastFile) -and (Get-Item $lastFile).LastWriteTimeUtc -gt $was)) {
            throw "the WPR-Stop task saved nothing within 120 s (see $taskDir\out\task.log)"
        }
        $etl = Join-Path $taskDir ('out\' + (Get-Content -LiteralPath $lastFile -TotalCount 1).Trim())
        Copy-Item -LiteralPath $etl -Destination (Join-Path $Out "$name.etl") -Force
        "$name <- $etl" | Add-Content -Path (Join-Path $Out 'task-files.txt') -Encoding ascii
        return
    }
    & wpr.exe -stop (Join-Path $Out "$name.etl") *> (Join-Path $Out "$name-stop.txt")
    $script:recording = $false
    if ($LASTEXITCODE -ne 0) { throw "wpr -stop failed (see $Out\$name-stop.txt)" }
}

# Samplers for the whole run: GPU clocks/power/throttle (nvidia-smi) and per-process engine load.
$samplers = @()
function Start-Samplers {
    $smi = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
    if ($smi) {
        $csv = Join-Path $Out 'nvsmi.csv'
        $samplers += Start-Process -FilePath $smi.Source -WindowStyle Hidden -PassThru -ArgumentList @(
            '--query-gpu=timestamp,pstate,clocks.gr,clocks.mem,power.draw,utilization.gpu,utilization.memory,clocks_event_reasons.active,temperature.gpu',
            '--format=csv', '-lms', '250', '-f', "`"$csv`"")
    } else { 'perf-gpu-trace: nvidia-smi not found - no clock/power samples' }
    $engCsv = Join-Path $Out 'gpu-engines.csv'
    $samplers += Start-Job -ArgumentList $engCsv -ScriptBlock {
        param($path)
        'tick,process,pid,engtype,util' | Set-Content -Path $path -Encoding ascii
        while ($true) {
            try {
                $s = Get-Counter -Counter '\GPU Engine(*)\Utilization Percentage' -SampleInterval 1 -MaxSamples 1 -ErrorAction Stop
                $tick = [uint32]([Environment]::TickCount -band 0x7fffffff)
                $procs = @{}
                Get-Process | ForEach-Object { $procs[$_.Id] = $_.ProcessName }
                $rows = @{}
                foreach ($c in $s.CounterSamples) {
                    if ($c.InstanceName -notmatch 'pid_(\d+)_.*engtype_(.+)$') { continue }
                    $pid_ = [int]$Matches[1]; $eng = $Matches[2]
                    $key = "$pid_|$eng"
                    if (-not $rows.ContainsKey($key)) { $rows[$key] = 0.0 }
                    $rows[$key] += $c.CookedValue
                }
                $lines = foreach ($k in $rows.Keys) {
                    if ($rows[$k] -lt 0.5) { continue }
                    $pid_, $eng = $k -split '\|', 2
                    $name = $procs[[int]$pid_]; if (-not $name) { $name = '?' }
                    '{0},{1},{2},{3},{4:N1}' -f $tick, $name, $pid_, $eng, $rows[$k]
                }
                if ($lines) { $lines | Add-Content -Path $path -Encoding ascii }
            } catch { Start-Sleep -Milliseconds 500 }
        }
    }
    return $samplers
}
function Stop-Samplers($list) {
    foreach ($s in $list) {
        if ($s -is [System.Diagnostics.Process]) { try { $s.Kill() } catch {} }
        elseif ($s) { Stop-Job $s -ErrorAction SilentlyContinue; Remove-Job $s -Force -ErrorAction SilentlyContinue }
    }
}

$windows = Join-Path $Out 'trace-windows.csv'
'name,startTick,stopTick,label' | Set-Content -Path $windows -Encoding ascii
$samplers = @()
try {
    if ($Smoke) {
        $samplers = Start-Samplers
        $t0 = Tick; Start-Rec; Start-Sleep -Seconds ([Math]::Max(2, $Seconds - 1)); Stop-Rec 'smoke'; $t1 = Tick
        "smoke,$t0,$t1,desktop" | Add-Content -Path $windows -Encoding ascii
        "perf-gpu-trace: smoke capture saved: $Out\smoke.etl ($([Math]::Round((Get-Item "$Out\smoke.etl").Length / 1MB, 1)) MB)"
        return
    }
    if (Get-Process Dishonored -ErrorAction SilentlyContinue) {
        'perf-gpu-trace: the game is already running - arming anyway (the plan may already be past some segments)'
    } else { 'perf-gpu-trace: ARMED - launch the game now; this window waits for it' }
    $deadline = (Get-Date).AddMinutes($WaitMinutes)
    $game = $null
    while (-not $game -and (Get-Date) -lt $deadline) {
        $game = Get-Process Dishonored -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $game) { Start-Sleep -Seconds 1 }
    }
    if (-not $game) { throw "No game within $WaitMinutes minutes." }
    "perf-gpu-trace: game pid $($game.Id) - samplers on, waiting for the A/B plan's segments"
    $samplers = Start-Samplers
    $log = Get-DvrLogPath
    Start-Sleep -Seconds 3
    $planArmed = -not $Plan
    if ($Plan) {
        $planName = Split-Path -Leaf $Plan
        Copy-Item -LiteralPath $Plan -Destination (Join-Path (Get-DvrDataDir) $planName) -Force
        Copy-Item -LiteralPath $Plan -Destination (Join-Path $Out $planName) -Force
    }
    $fs = [IO.File]::Open($log, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    $reader = New-Object IO.StreamReader($fs)
    $captures = 0
    $pending = $null     # @{ k; label; due }
    $done = $false
    while (-not $game.HasExited -and -not $done) {
        while (($line = $reader.ReadLine()) -ne $null) {
            if ($line -match 'perf/ab: segment (\d+) of (\d+): (.+)$') {
                $k = [int]$Matches[1]; $label = $Matches[3].Trim()
                if ($captures -lt $MaxCaptures -and $label -match $Segments) {
                    $pending = @{ k = $k; label = $label; due = (Get-Date).AddSeconds($IntoSegment) }
                    "perf-gpu-trace: segment $k ($label) - tracing $Seconds s from +$IntoSegment s"
                } else { $pending = $null }
            } elseif ($line -match 'perf/ab: PLAN COMPLETE') { $done = $true }
            elseif (-not $planArmed -and $line -match 'xr: pipeline READY') {
                Start-Sleep -Seconds 2
                [IO.File]::WriteAllText((Get-DvrCmdPath), "perf ab plan $planName`n")
                $planArmed = $true
                "perf-gpu-trace: armed the A/B plan $planName through the seam - load the save and stand still"
            }
        }
        if ($pending -and (Get-Date) -ge $pending.due) {
            $safe = ($pending.label -replace '[^A-Za-z0-9]+', '-').Trim('-')
            $name = 'seg{0:D2}-{1}' -f $pending.k, $safe
            $t0 = Tick; Start-Rec; Start-Sleep -Seconds $Seconds; Stop-Rec $name; $t1 = Tick
            "$name,$t0,$t1,$($pending.label)" | Add-Content -Path $windows -Encoding ascii
            $captures++
            "perf-gpu-trace: saved $name.etl ($([Math]::Round((Get-Item "$Out\$name.etl").Length / 1MB, 1)) MB)"
            $pending = $null
        }
        Start-Sleep -Milliseconds 250
    }
    $reader.Dispose()
    if ($done) { 'perf-gpu-trace: the A/B plan is COMPLETE - you can quit the game now' }
    Start-Sleep -Seconds 2
} finally {
    if ($script:recording) { try { Stop-Rec 'aborted' } catch { $_ | Out-String | Add-Content (Join-Path $Out 'error.txt') } }
    Stop-Samplers $samplers
    if (-not $Smoke) {
        try { Copy-Item -LiteralPath (Get-DvrLogPath) -Destination (Join-Path $Out 'dishonored_vr.log') -Force } catch {}
    }
    "perf-gpu-trace: done - $Out"
}
