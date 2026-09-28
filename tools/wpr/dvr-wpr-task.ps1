# The elevated half of the trace recorder. It runs ONLY as a scheduled task registered by
# tools\perf-trace-task-setup.ps1, from an admin-only folder (C:\Program Files\DishonoredVR-Trace), so
# a process at user level can trigger a WPR start or stop but cannot change what runs elevated.
# It takes no path from anyone: the profile is its own copy of dvr-gpu.wprp, the output folder is fixed.
param(
    [ValidateSet('start', 'stop', 'cancel', 'status')][string]$Action = 'status',
    [ValidateSet('DvrGpu', 'DvrGpuStacks')][string]$Profile = 'DvrGpu'
)
$ErrorActionPreference = 'Continue'
$dir = $PSScriptRoot
$out = Join-Path $dir 'out'
$wprp = Join-Path $dir 'dvr-gpu.wprp'
$log = Join-Path $out 'task.log'
function Note([string]$s) { "{0:yyyy-MM-dd HH:mm:ss.fff} {1}" -f (Get-Date), $s | Add-Content -Path $log -Encoding ascii }
function Status {
    $s = & wpr.exe -status 2>&1 | Out-String
    $state = if ($s -match 'WPR is not recording') { 'idle' } elseif ($s -match 'recording') { 'recording' } else { 'unknown' }
    "$state`r`n$s" | Set-Content -Path (Join-Path $out 'status.txt') -Encoding ascii
    return $state
}
New-Item -ItemType Directory -Force $out | Out-Null
switch ($Action) {
    'start' {
        $free = (Get-PSDrive -Name ($dir.Substring(0, 1))).Free
        $used = (Get-ChildItem $out -Filter *.etl -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum
        if ($free -lt 4GB -or $used -gt 8GB) { Note "start REFUSED: free $([int]($free/1GB)) GB, traces $([int]($used/1GB)) GB"; Status | Out-Null; break }
        if ((Status) -ne 'idle') { Note 'start REFUSED: WPR is not idle (another recording is running)'; break }
        $r = & wpr.exe -start "$wprp!$Profile" -filemode 2>&1 | Out-String
        Note "start $Profile exit=$LASTEXITCODE $($r.Trim())"
        Status | Out-Null
    }
    'stop' {
        $name = 'dvr-{0:yyyyMMdd-HHmmss-fff}.etl' -f (Get-Date)
        $r = & wpr.exe -stop (Join-Path $out $name) 2>&1 | Out-String
        Note "stop -> $name exit=$LASTEXITCODE $($r.Trim())"
        if ($LASTEXITCODE -eq 0) { $name | Set-Content -Path (Join-Path $out 'last.txt') -Encoding ascii }
        Status | Out-Null
    }
    'cancel' {
        $r = & wpr.exe -cancel 2>&1 | Out-String
        Note "cancel exit=$LASTEXITCODE $($r.Trim())"
        Status | Out-Null
    }
    default { Status | Out-Null }
}
