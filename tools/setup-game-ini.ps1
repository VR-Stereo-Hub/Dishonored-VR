# setup-game-ini.ps1 - the one-time game config the mod needs.
#
#   .\setup-game-ini.ps1 -VRBaseline            the game-side settings the mod expects
#   .\setup-game-ini.ps1 -Console               enable the console (F1 in game, then ~)
#   .\setup-game-ini.ps1 -Restore               put the newest backup back
#
# Edits %USERPROFILE%\Documents\My Games\Dishonored\DishonoredGame\Config\
# DishonoredInput.ini after a timestamped backup.
#
# There is no resolution step any more (41.0): the game renders natively at
# whatever window size the player picks in its own options, and the per-eye
# renders go to offscreen targets. If a release before 41.0 left ResX=4032
# ResY=2268 in DishonoredEngine.ini and all four [AppCompatBucketN] sections
# of DishonoredCompat.ini, set them back to a size your monitor has (the
# in-game video options do it) or run -Restore if the backups are still there.
#
# Ships in the release zip; PowerShell 5.1, pure ASCII, CRLF.
param(
    [switch]$VRBaseline,
    [switch]$Console,
    [switch]$Restore,
    [string]$ConfigDir = ""
)
$ErrorActionPreference = 'Stop'
if (-not $ConfigDir) {
    $ConfigDir = Join-Path ([Environment]::GetFolderPath('MyDocuments')) "My Games\Dishonored\DishonoredGame\Config"
}
if (-not (Test-Path $ConfigDir)) { throw "config folder not found: $ConfigDir - run the game once first." }
$engine = Join-Path $ConfigDir "DishonoredEngine.ini"
$compat = Join-Path $ConfigDir "DishonoredCompat.ini"
$input = Join-Path $ConfigDir "DishonoredInput.ini"

function Backup($path) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $bak = "$path.$stamp.dvr-backup"
    Copy-Item $path $bak
    Write-Host "backup: $bak"
}

if ($Restore) {
    foreach ($p in @($engine, $compat, $input)) {
        $bak = Get-ChildItem "$p.*.dvr-backup" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
        if ($bak) { Copy-Item $bak.FullName $p -Force; Write-Host "restored $(Split-Path -Leaf $p) from $($bak.Name)" }
    }
    return
}

# Set one key inside ONE section. The same key name lives in more than one
# section of DishonoredEngine.ini - Fullscreen and DepthOfField are both in
# [SystemSettings] and [SystemSettingsMobile] - so a file-wide match would edit
# the wrong line and leave the real one untouched. Returns $true if it changed
# anything. Reports what it saw, so a drifted value is visible rather than
# silently overwritten.
function Set-IniKeyInSection($path, $section, $key, $value) {
    $lines = [System.IO.File]::ReadAllLines($path)
    $inSection = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -match '^\s*\[(.+)\]\s*$') {
            $inSection = ($matches[1] -eq $section)
            continue
        }
        if (-not $inSection) { continue }
        if ($line -match "^\s*$([regex]::Escape($key))\s*=\s*(.*?)\s*$") {
            $had = $matches[1]
            if ($had -ceq $value) {
                Write-Host ("  [{0}] {1}={2} already set" -f $section, $key, $value)
                return $false
            }
            $lines[$i] = "$key=$value"
            Write-Host ("  [{0}] {1}: {2} -> {3}" -f $section, $key, $had, $value)
            [System.IO.File]::WriteAllLines($path, $lines)
            return $true
        }
    }
    throw "could not find $key in [$section] of $path - the file is not the shape this expects, nothing was written"
}

if ($VRBaseline) {
    # The game-side settings the mod expects, and why each one is here. These
    # are the ONLY four; everything else the mod drives itself, so do not add
    # to this list without a measured reason.
    #
    #   bSmoothFrameRate       the engine's frame smoothing fights the headset's
    #                          own pacing; leaving it on produces judder the mod
    #                          cannot correct from outside.
    #   DepthOfField           a post effect applied to a MONO image; in stereo
    #                          it blurs by screen position, not by eye depth.
    #   UseVsync               the mod presents without vsync anyway via
    #                          [Perf] ForceNoVSync, and leaving the engine's on
    #                          adds a second wait against the compositor's.
    #   bEnableMouseSmoothing  head tracking writes ProcessViewRotation directly,
    #                          but mouse emulation is still the fallback path,
    #                          and smoothing there lags the head.
    #
    # NOT set here: [SystemSettings] Fullscreen. VirtualMode creates the device
    # windowed with the backbuffer kept whatever it says, so forcing it would be
    # a line that changes nothing and drifts back after any visit to the game's
    # video options.
    Backup $engine
    Backup $input
    $changed = $false
    Write-Host "VR baseline:"
    $changed = (Set-IniKeyInSection $engine "Engine.Engine"     "bSmoothFrameRate"      "FALSE") -or $changed
    $changed = (Set-IniKeyInSection $engine "SystemSettings"    "DepthOfField"          "False") -or $changed
    $changed = (Set-IniKeyInSection $engine "SystemSettings"    "UseVsync"              "False") -or $changed
    $changed = (Set-IniKeyInSection $input  "Engine.PlayerInput" "bEnableMouseSmoothing" "FALSE") -or $changed
    if ($changed) {
        Write-Host "VR baseline applied. The game REWRITES these when you use its own video"
        Write-Host "options, so re-run this after visiting them."
    } else {
        Write-Host "VR baseline already in place, nothing written."
    }
}

if ($Console) {
    Backup $input
    $lines = Get-Content $input
    $bind = 'm_PCBindings=(Name="F1",Command="set Console ConsoleKey Tilde | set PlayerController CheatClass class' + "'" + 'DishonoredCheatManager' + "'" + ' | EnableCheats")'
    if ($lines -contains $bind) { Write-Host "console bind already present"; return }
    $idx = [Array]::FindIndex($lines, [Predicate[string]]{ param($l) $l -match 'm_PCBindings=\(Name="Zero"' })
    if ($idx -lt 0) { throw "could not find the m_PCBindings block in $input" }
    $out = @($lines[0..$idx]) + @($bind) + @($lines[($idx + 1)..($lines.Count - 1)])
    [System.IO.File]::WriteAllLines($input, [string[]]$out)
    Write-Host "console bind added after the Zero binding: press F1 in game once, then ~ opens the console"
}

if (-not $Console -and -not $VRBaseline) {
    Write-Host "nothing to do - pass -VRBaseline or -Console (or -Restore)"
}
