# status-dump.ps1 - ask the running mod for its status and print it.
# Sends `status` through the command seam, waits for the acknowledgement in
# ack.txt, then pretty-prints %LOCALAPPDATA%\DishonoredVR\status.json.
#   .\tools\status-dump.ps1            # full JSON
#   .\tools\status-dump.ps1 -Raw       # the file as written
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([switch]$Raw, [int]$WaitSeconds = 5)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$ack = Get-DvrAckPath
$status = Get-DvrStatusPath
$before = if (Test-Path $ack) { (Get-Item $ack).LastWriteTimeUtc } else { [datetime]::MinValue }
& (Join-Path $PSScriptRoot "game-cmd.ps1") -NoFocus "status" | Out-Null
$deadline = (Get-Date).AddSeconds($WaitSeconds)
while ((Get-Date) -lt $deadline) {
    if ((Test-Path $ack) -and (Get-Item $ack).LastWriteTimeUtc -gt $before) { break }
    Start-Sleep -Milliseconds 200
}
if (-not (Test-Path $status)) { throw "no status.json at $status - is the game running with the mod?" }
if ($Raw) { Get-Content $status -Raw; return }
$s = Get-Content $status -Raw | ConvertFrom-Json
$s | ConvertTo-Json -Depth 6
