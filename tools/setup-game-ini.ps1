# setup-game-ini.ps1 - the one-time game config the mod needs. Replaces the
# setup_resolution.bat of the original release.
#
#   .\setup-game-ini.ps1 -Resolution            ResX=4032 ResY=2268 Fullscreen=False
#   .\setup-game-ini.ps1 -Resolution -Width 3200 -Height 1800
#   .\setup-game-ini.ps1 -Console               enable the console (F1 in game, then ~)
#   .\setup-game-ini.ps1 -Restore               put the newest backup back
#
# Edits %USERPROFILE%\Documents\My Games\Dishonored\DishonoredGame\Config\
# DishonoredEngine.ini and DishonoredInput.ini, after a timestamped backup.
# The mod renders both eyes side by side into one 4032x2268 window (2016x2268
# per eye), so the game must run windowed at that size. Ships in the release
# zip; PowerShell 5.1, pure ASCII, CRLF.
param(
    [switch]$Resolution,
    [int]$Width = 4032,
    [int]$Height = 2268,
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
