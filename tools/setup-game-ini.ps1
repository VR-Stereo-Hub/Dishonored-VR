# setup-game-ini.ps1 - the one-time game config the mod needs. Replaces the
# setup_resolution.bat of the original release.
#
#   .\setup-game-ini.ps1 -Resolution            ResX=2850 ResY=2750 Fullscreen=False
#   .\setup-game-ini.ps1 -Resolution -Width 3200 -Height 1800
#   .\setup-game-ini.ps1 -Console               enable the console (F1 in game, then ~)
#   .\setup-game-ini.ps1 -Restore               put the newest backup back
#
# Edits %USERPROFILE%\Documents\My Games\Dishonored\DishonoredGame\Config\
# DishonoredEngine.ini, DishonoredCompat.ini and DishonoredInput.ini, after a
# timestamped backup. The mod renders both eyes side by side into one window,
# so the game must run windowed at that size.
#
# WHY DishonoredCompat.ini IS EDITED TOO (measured 2026-09-01): setting ResX in
# DishonoredEngine.ini alone DOES NOT HOLD. Dishonored runs UE3's AppCompat
# hardware detection at startup, picks an [AppCompatBucketN], and writes that
# bucket's ResX/ResY straight over [SystemSettings]. Buckets 3 and 4 ship
# 1600x900, and any GPU newer than the 2012 lookup table falls into one of them,
# so a modern card silently lands on 1600x900 on EVERY launch no matter what the
# engine ini says. Setting the buckets makes AppCompat write the right number
# itself. The original setup_resolution.bat only touched the engine ini, which
# is why "the resolution keeps reverting" was never fixed by re-running it.
#
# THE RENDER MUST BE LANDSCAPE (measured 2026-09-01, and this is not cosmetic).
# The DXVK fork refuses to splice the MAIN SCENE unless the viewport is wider
# than it is tall - d3d9_device.cpp gives the refusal the reason "rt-portrait",
# and the per-eye splice at the bottom of DrawIndexedPrimitive runs only when
# that reason is "SPLICE". Effects passes (light shafts, shadows, the M8.1
# quarter light pass) splice under DIFFERENT conditions and keep working, so
# the fork's splice counter stays high and everything downstream still believes
# the frame is stereo.
#
# The result of a portrait render is the worst possible failure: the world is
# drawn MONO across the full frame while the proxy hands each eye a different
# HALF of it. The two eyes get unrelated views that cannot fuse, and each half
# is stretched across the whole quad, so it is also magnified 2x. That is the
# "the eyes are super far off and both are zoomed in" report, exactly.
#
# 2850x2750 is 2750x2850 with the two numbers swapped: identical pixel cost,
# landscape by 100 px so the gate passes, full-frame aspect 1.036 so the eye
# quad subtends 100 x 98 deg at FovLever=100 - right for a Quest 3. If you
# change these, keep Width > Height.
#
# Ships in the release zip; PowerShell 5.1, pure ASCII, CRLF.
param(
    [switch]$Resolution,
    [int]$Width = 2850,
    [int]$Height = 2750,
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
$input = Join-Path $ConfigDir "DishonoredInput.ini"

function Backup($path) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $bak = "$path.$stamp.dvr-backup"
    Copy-Item $path $bak
    Write-Host "backup: $bak"
}

function Set-IniKey($path, $key, $value) {
    # Replace every occurrence of the key (UE3 inis repeat keys per section);
    # the resolution keys live under [SystemSettings] in Dishonored's ini.
    $lines = Get-Content $path
    $n = 0
    $out = foreach ($l in $lines) {
        if ($l -match "^\s*$([regex]::Escape($key))\s*=") { $n++; "$key=$value" } else { $l }
    }
    if ($n -eq 0) { throw "$key not found in $path - the file layout is not what this script expects; edit it by hand." }
    [System.IO.File]::WriteAllLines($path, [string[]]$out)
    Write-Host "$(Split-Path -Leaf $path): $key=$value ($n line(s))"
}

if ($Restore) {
    foreach ($p in @($engine, $input)) {
        $bak = Get-ChildItem "$p.*.dvr-backup" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
        if ($bak) { Copy-Item $bak.FullName $p -Force; Write-Host "restored $(Split-Path -Leaf $p) from $($bak.Name)" }
    }
    return
}

if ($Resolution) {
    Backup $engine
    Set-IniKey $engine "ResX" $Width
    Set-IniKey $engine "ResY" $Height
    Set-IniKey $engine "Fullscreen" "False"

    # The AppCompat buckets decide the STARTUP resolution and overwrite the
    # engine ini every launch (see the header). Set all four, because which one
    # a machine lands in depends on a 2012 GPU table we cannot predict.
    $compat = Join-Path $ConfigDir "DishonoredCompat.ini"
    if (Test-Path $compat) {
        Backup $compat
        $lines = Get-Content $compat
        $sec = ""; $n = 0
        $out = foreach ($l in $lines) {
            if ($l -match '^\s*\[(.+)\]\s*$') { $sec = $Matches[1]; $l }
            elseif ($sec -like 'AppCompatBucket*' -and $l -match '^\s*ResX\s*=') { $n++; "ResX=$Width" }
            elseif ($sec -like 'AppCompatBucket*' -and $l -match '^\s*ResY\s*=') { $n++; "ResY=$Height" }
            else { $l }
        }
        [System.IO.File]::WriteAllLines($compat, [string[]]$out)
        Write-Host "DishonoredCompat.ini: AppCompat buckets set to ${Width}x${Height} ($n line(s))"
        if ($n -eq 0) {
            Write-Warning "no AppCompatBucket ResX/ResY lines found - if the game keeps starting at 1600x900, this is why."
        }
    } else {
        Write-Warning "DishonoredCompat.ini not found. The game may overwrite ResX/ResY at startup from its AppCompat bucket; run the game once, then re-run this."
    }
    Write-Host "Launch the game normally; the mod's window hooks keep the size. Alt+Enter toggles fullscreen back off if needed."
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

if (-not ($Resolution -or $Console)) {
    Write-Host "nothing to do - pass -Resolution and/or -Console (or -Restore)"
}
