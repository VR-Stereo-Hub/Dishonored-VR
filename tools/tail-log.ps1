# Follows dishonored_vr.log live (it sits next to Dishonored.exe).
#   .\tools\tail-log.ps1                 # last 50 lines, then follow
#   .\tools\tail-log.ps1 -Grep "xr:|crash"   # only matching lines
#   .\tools\tail-log.ps1 -Since 500      # start further back
# Read-only, never touches the headset.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [string]$GamePath = "",
    [string]$Grep = "",
    [int]$Since = 50
)
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$log = Get-DvrLogPath $GamePath
if (-not (Test-Path $log)) {
    Write-Host "No log yet at $log - launch the game with the mod installed first."
    Write-Host "Waiting for it to appear..."
    while (-not (Test-Path $log)) { Start-Sleep -Milliseconds 500 }
}
if ($Grep) {
    Get-Content $log -Wait -Tail $Since | Where-Object { $_ -match $Grep }
} else {
    Get-Content $log -Wait -Tail $Since
}
