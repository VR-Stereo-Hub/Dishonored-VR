# game-cmd.ps1 - write one or more commands to the mod's command seam
# (%LOCALAPPDATA%\DishonoredVR\command.txt, polled at 1 Hz from the Present
# hook; see src/core/framework/command.h for the vocabulary).
#
# Retries past the transient share-lock while the mod has the file open.
# Foregrounds the game first: it costs nothing, and the game may pause its
# render loop while unfocused ([Screen] KeepAliveUnfocused=0).
# Uses WriteAllText, never Set-Content -Encoding utf8, whose BOM would corrupt
# the first token.
#
# Usage: .\tools\game-cmd.ps1 "status"
#        .\tools\game-cmd.ps1 "log cat blink debug" "recenter"
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
[CmdletBinding(PositionalBinding=$false)]
param(
    [switch]$NoFocus,
    [Parameter(ValueFromRemainingArguments=$true)][string[]]$Lines
)
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Cmd { [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); }
'@
if (-not $NoFocus) {
    $p = @(Get-Process Dishonored -ErrorAction SilentlyContinue | Where-Object {
        -not $_.HasExited -and $_.MainWindowHandle -ne [IntPtr]::Zero
    } | Sort-Object Id -Descending) | Select-Object -First 1
    if ($p -and $p.MainWindowHandle -ne [IntPtr]::Zero) {
        [Cmd]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
        Start-Sleep -Milliseconds 400
    }
}
$cmd = Get-DvrCmdPath
$cmdDir = Split-Path -Parent $cmd
if (-not (Test-Path $cmdDir)) { New-Item -ItemType Directory -Force $cmdDir | Out-Null }
$text = ($Lines -join "`n")
for ($i = 0; $i -lt 30; $i++) {
    try {
        [System.IO.File]::WriteAllText($cmd, $text + "`n")
        "wrote $($Lines.Count) command(s) to $cmd"
        return
    } catch {
        Start-Sleep -Milliseconds 200
    }
}
throw "could not write command.txt after retries"
