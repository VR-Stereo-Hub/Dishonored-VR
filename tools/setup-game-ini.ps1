# setup-game-ini.ps1 - the one-time game config the mod needs.
#
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

if (-not $Console) {
    Write-Host "nothing to do - pass -Console (or -Restore)"
}
