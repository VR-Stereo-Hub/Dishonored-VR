# boot.ps1 - boot Dishonored into GAMEPLAY on the newest save, unattended.
#
# Launches through Steam (or -Attach to a process tools\xrsim-launch.ps1 already
# started with XR_RUNTIME_JSON set - mandatory in sim mode, or Steam starts a
# SECOND Dishonored.exe on the real runtime), waits for the game window,
# foregrounds it, then presses Enter every ~3.5 s to pass "press any key" and
# take the menu's default (Continue). NEVER New Game: the prologue is broken
# with the mod (docs/KNOWN_ISSUES.md) and would trip the IntroSkip workaround.
#
# The gameplay detector is the mod's own "[game] state: GAMEPLAY" log line
# (src/game/dishonored/game_state.cpp: live pawn latched, no cinematic, menu
# closed). Older builds: falls back to "handmesh: latched pawn" followed by
# "menu: closed".
#
# UNVERIFIED against the real game as of 2026-09-02 (not installed on the dev
# PC): the menu key sequence and the dialog handling are the BioShock harness's
# shape and need one attended run to confirm. See docs/dishonored/TESTING.md.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [switch]$Attach,
    [string]$GamePath = "",
    [int]$MaxPresses = 70
)

$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$log = Get-DvrLogPath $GamePath
$proc = "Dishonored"

Add-Type @'
using System; using System.Runtime.InteropServices; using System.Text;
public static class W {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
'@

if ($Attach) {
    if (-not (Get-Process $proc -ErrorAction SilentlyContinue)) {
        "FAIL: -Attach was given but $proc is not running"
        exit 1
    }
    "attaching to the running $proc (no Steam launch)"
} else {
    & (Join-Path $PSScriptRoot "launch-game.ps1") -GamePath $GamePath | Out-Null
    "launched via steam"
}

# Phase 1: wait for the game window.
$game = $null
for ($i = 0; $i -lt 120; $i++) {
    $p = Get-Process $proc -ErrorAction SilentlyContinue
    if ($p -and $p.MainWindowHandle -ne 0) { $game = $p; break }
    Start-Sleep -Milliseconds 1000
}
if (-not $game) { "FAIL: game window never appeared"; exit 1 }
"game window up (pid $($game.Id)), foregrounding"
[W]::SetForegroundWindow($game.MainWindowHandle) | Out-Null
Start-Sleep -Seconds 3

function Test-Gameplay {
    if (-not (Test-Path $log)) { return $false }
    if (Select-String -Path $log -Pattern "[game] state: GAMEPLAY" -SimpleMatch -Quiet -ErrorAction SilentlyContinue) { return $true }
    $latched = Select-String -Path $log -Pattern "handmesh: latched pawn" -SimpleMatch -Quiet -ErrorAction SilentlyContinue
    $closed = Select-String -Path $log -Pattern "menu: closed" -SimpleMatch -Quiet -ErrorAction SilentlyContinue
    return ($latched -and $closed)
}

# Phase 2: Enter-press loop until the mod reports gameplay.
for ($i = 0; $i -lt $MaxPresses; $i++) {
    if (Test-Gameplay) {
        "GAMEPLAY REACHED (after $i presses)"
        exit 0
    }
    $p = Get-Process $proc -ErrorAction SilentlyContinue
    if (-not $p) { "FAIL: game process died"; exit 1 }
    [W]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
    & (Join-Path $PSScriptRoot "game-key.ps1") Enter | Out-Null
    Start-Sleep -Milliseconds 3500
}
"FAIL: gameplay never reached (timed out after $MaxPresses presses)"
exit 1
