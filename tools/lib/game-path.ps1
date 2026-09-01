# game-path.ps1 - where Dishonored, its config and the mod's files live.
# Dot-source this from every script that touches the game:
#   . (Join-Path $PSScriptRoot "lib\game-path.ps1")
#
# Resolution order for the game folder (the one holding Dishonored.exe):
#   1. $env:DVR_GAME_DIR                (explicit; also what CI and a second
#                                        install would set)
#   2. Steam's libraryfolders.vdf        (every library, checked for
#                                        appmanifest_205100.acf)
#   3. throw                             (the game is not installed here)
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

$script:DvrAppId = "205100"

function Get-DvrGamePath {
    param([string]$Override = "")
    if ($Override) {
        if (Test-Path (Join-Path $Override "Dishonored.exe")) { return $Override }
        throw "Dishonored.exe not found in '$Override'."
    }
    if ($env:DVR_GAME_DIR) {
        if (Test-Path (Join-Path $env:DVR_GAME_DIR "Dishonored.exe")) { return $env:DVR_GAME_DIR }
        throw "DVR_GAME_DIR is set to '$($env:DVR_GAME_DIR)' but Dishonored.exe is not there."
    }
    $steam = $null
    try { $steam = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -ErrorAction Stop).SteamPath } catch {}
    if (-not $steam) { $steam = "C:\Program Files (x86)\Steam" }
    $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
    $libs = @($steam)
    if (Test-Path $vdf) {
        foreach ($line in Get-Content $vdf) {
            if ($line -match '^\s*"path"\s+"(.+)"') { $libs += $Matches[1].Replace('\\', '\') }
        }
    }
    foreach ($lib in $libs | Select-Object -Unique) {
        $acf = Join-Path $lib "steamapps\appmanifest_$script:DvrAppId.acf"
        if (-not (Test-Path $acf)) { continue }
        $dir = Join-Path $lib "steamapps\common\Dishonored\Binaries\Win32"
        if (Test-Path (Join-Path $dir "Dishonored.exe")) { return $dir }
    }
    throw "Dishonored (Steam app $script:DvrAppId) is not installed in any Steam library. Set DVR_GAME_DIR to the folder holding Dishonored.exe."
}

# %USERPROFILE%\Documents\My Games\Dishonored\DishonoredGame\Config
function Get-DvrConfigDir {
    $docs = [Environment]::GetFolderPath('MyDocuments')
    return Join-Path $docs "My Games\Dishonored\DishonoredGame\Config"
}

# Harness and bulk files (command.txt, ack.txt, status.json, dumps, xrsim).
function Get-DvrDataDir {
    if ($env:DVR_DATA_DIR) { return $env:DVR_DATA_DIR }
    return Join-Path $env:LOCALAPPDATA "DishonoredVR"
}

# User-facing files stay next to the exe (see src/core/util/paths.h).
function Get-DvrLogPath { param([string]$GamePath = "") return Join-Path (Get-DvrGamePath $GamePath) "dishonored_vr.log" }
function Get-DvrIniPath { param([string]$GamePath = "") return Join-Path (Get-DvrGamePath $GamePath) "dishonored_vr.ini" }
function Get-DvrCmdPath { return Join-Path (Get-DvrDataDir) "command.txt" }
function Get-DvrAckPath { return Join-Path (Get-DvrDataDir) "ack.txt" }
function Get-DvrStatusPath { return Join-Path (Get-DvrDataDir) "status.json" }

# Same PE read as xrsim-install.ps1: refuse a 64-bit DLL before it is copied
# next to a 32-bit game and silently ignored.
function Assert-DvrX86Dll {
    param([Parameter(Mandatory)][string]$Path)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        $fs.Seek(0x3C, 'Begin') | Out-Null
        $peOffset = $br.ReadInt32()
        $fs.Seek($peOffset, 'Begin') | Out-Null
        if ($br.ReadUInt32() -ne 0x00004550) { throw "$Path is not a PE image." }
        $machine = $br.ReadUInt16()
    } finally { $fs.Close() }
    if ($machine -ne 0x014C) { throw ("$Path reports PE machine 0x{0:X4}, not 0x014C (x86)." -f $machine) }
}
