# One-time setup (run ELEVATED) so the trace recorder can start and stop WPR later without a UAC
# prompt: copies the task script and the WPR profile into an admin-only folder and registers four
# on-demand scheduled tasks under \DishonoredVR\ that run them with highest privileges for this user.
#
#   .\tools\perf-trace-task-setup.ps1           # install (or refresh after the profile changes)
#   .\tools\perf-trace-task-setup.ps1 -Remove   # unregister the tasks and delete the folder
#
# What runs elevated is fixed at setup: C:\Program Files\DishonoredVR-Trace\dvr-wpr-task.ps1 with a
# fixed action (start|stop|cancel), its own copy of dvr-gpu.wprp, output into its own out\ folder.
# Users can read that folder (to collect the .etl files) but cannot change anything in it.
[CmdletBinding()]
param([switch]$Remove)
$ErrorActionPreference = 'Stop'
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) { throw 'Run this once from an elevated (Administrator) PowerShell.' }
$dir = Join-Path $env:ProgramFiles 'DishonoredVR-Trace'
$taskPath = '\DishonoredVR\'
$tasks = @{
    'WPR-Start'       = '-Action start -Profile DvrGpu'
    'WPR-StartStacks' = '-Action start -Profile DvrGpuStacks'
    'WPR-Stop'        = '-Action stop'
    'WPR-Cancel'      = '-Action cancel'
}
if ($Remove) {
    foreach ($t in $tasks.Keys) { Unregister-ScheduledTask -TaskPath $taskPath -TaskName $t -Confirm:$false -ErrorAction SilentlyContinue }
    if (Test-Path $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
    "perf-trace-task-setup: removed the tasks and $dir"
    return
}
New-Item -ItemType Directory -Force $dir, (Join-Path $dir 'out') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'wpr\dvr-gpu.wprp') -Destination $dir -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'wpr\dvr-wpr-task.ps1') -Destination $dir -Force
# Admin-only writes: Administrators and SYSTEM full control, Users read and execute. No inheritance
# from Program Files, so nothing a user can write reaches the elevated script.
$acl = New-Object System.Security.AccessControl.DirectorySecurity
$acl.SetAccessRuleProtection($true, $false)
$inh = [System.Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit'
$none = [System.Security.AccessControl.PropagationFlags]::None
foreach ($sid in 'S-1-5-32-544', 'S-1-5-18') {
    $id = (New-Object System.Security.Principal.SecurityIdentifier $sid).Translate([System.Security.Principal.NTAccount])
    $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($id, 'FullControl', $inh, $none, 'Allow')))
}
$users = (New-Object System.Security.Principal.SecurityIdentifier 'S-1-5-32-545').Translate([System.Security.Principal.NTAccount])
$acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($users, 'ReadAndExecute', $inh, $none, 'Allow')))
Set-Acl -LiteralPath $dir -AclObject $acl
$me = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
$principal = New-ScheduledTaskPrincipal -UserId $me -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit (New-TimeSpan -Minutes 5) -MultipleInstances IgnoreNew
$script = Join-Path $dir 'dvr-wpr-task.ps1'
foreach ($t in $tasks.Keys) {
    $action = New-ScheduledTaskAction -Execute 'powershell.exe' `
        -Argument "-NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$script`" $($tasks[$t])"
    Register-ScheduledTask -TaskPath $taskPath -TaskName $t -Action $action -Principal $principal -Settings $settings `
        -Description 'Dishonored VR performance trace (tools\perf-trace-task-setup.ps1). On demand only; -Remove undoes it.' `
        -Force | Out-Null
}
"perf-trace-task-setup: installed $dir and tasks $taskPath{WPR-Start, WPR-StartStacks, WPR-Stop, WPR-Cancel} for $me"
"Test (no admin needed): schtasks /run /tn `"\DishonoredVR\WPR-Start`" ; then WPR-Stop ; the .etl lands in $dir\out"
