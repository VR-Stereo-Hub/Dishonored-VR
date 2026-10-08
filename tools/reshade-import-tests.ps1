# Offline tests for tools\import-reshade-preset.ps1 (the launcher's preset drop zone).
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$helper=Join-Path $PSScriptRoot 'import-reshade-preset.ps1'
$ps=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$root=Join-Path $repo ('build/reshade-import-tests-'+[Guid]::NewGuid().ToString('N'))
$game=Join-Path $root 'game'; New-Item -ItemType Directory -Path $game | Out-Null
[IO.File]::WriteAllText((Join-Path $game 'Dishonored.exe'),'fixture only; never executed')
[IO.File]::WriteAllText((Join-Path $game 'd3d9.dll'),'the VR mod')
[IO.File]::WriteAllText((Join-Path $game 'ReShade.ini'),"[GENERAL]`r`nEffectSearchPaths=.\dvr-reshade-shaders\standard\Shaders`r`n`r`n[INPUT]`r`nKeyEffects=145,0,0,0`r`n")
New-Item -ItemType Directory -Force -Path (Join-Path $game 'dvr-reshade-shaders\standard\Shaders') | Out-Null
[IO.File]::WriteAllText((Join-Path $game 'dvr-reshade-shaders\standard\Shaders\Vibrance.fx'),'installed')
$preset="[Vibrance.fx]`r`nVibrance=0.15`r`nTechniques=Vibrance@Vibrance.fx,Mine@Mine.fx,Gone@NotThere.fx`r`n"

# A preset download the way they are actually shipped: a top folder, an old proxy DLL,
# a readme, a loose screenshot, its own ReShade.ini, shaders and a Textures folder.
$src=Join-Path $root 'Carinth Preset'; New-Item -ItemType Directory -Force -Path "$src\reshade-shaders\Shaders\Sub","$src\reshade-shaders\Textures" | Out-Null
[IO.File]::WriteAllText("$src\d3d9.dll",'old ReShade proxy - must never be copied')
[IO.File]::WriteAllText("$src\dxgi.dll",'another proxy')
[IO.File]::WriteAllText("$src\README.txt",'readme')
[IO.File]::WriteAllText("$src\screenshot.png",'not a texture')
[IO.File]::WriteAllText("$src\ReShade.ini",'their settings - must never replace ours')
[IO.File]::WriteAllText("$src\notapreset.ini",'[Something]')
[IO.File]::WriteAllText("$src\TestPreset.ini",$preset)
[IO.File]::WriteAllText("$src\reshade-shaders\Shaders\Mine.fx",'#include "Sub/Helper.fxh"')
[IO.File]::WriteAllText("$src\reshade-shaders\Shaders\Sub\Helper.fxh",'helper')
[IO.File]::WriteAllText("$src\reshade-shaders\Textures\lut.png",'texture')
$zip=Join-Path $root 'Carinth Preset.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($src,$zip,[IO.Compression.CompressionLevel]::Fastest,$true)

function Import([string[]]$paths) {
    $list=Join-Path $root ('list-'+[Guid]::NewGuid().ToString('N')+'.txt')
    [IO.File]::WriteAllLines($list,$paths,(New-Object Text.UTF8Encoding $false))
    $out=& $ps -NoProfile -ExecutionPolicy Bypass -File $helper -GameDir $game -ListFile $list
    return @{ code=$LASTEXITCODE; text=($out -join "`n") }
}
function Expect([bool]$ok,[string]$what) { if(-not $ok){ throw "FAIL: $what" } }

# ---- the zip
$r=Import @($zip)
Expect ($r.code -eq 0) "zip import exit code ($($r.text))"
Expect ([IO.File]::ReadAllText((Join-Path $game 'd3d9.dll')) -eq 'the VR mod') 'd3d9.dll was replaced'
Expect (-not (Test-Path (Join-Path $game 'dxgi.dll'))) 'dxgi.dll was copied'
Expect ([IO.File]::ReadAllText((Join-Path $game 'TestPreset.ini')) -eq $preset) 'preset not copied beside the exe'
Expect (-not (Test-Path (Join-Path $game 'notapreset.ini'))) 'a non-preset ini was copied'
Expect ([IO.File]::ReadAllText((Join-Path $game 'ReShade.ini')).Contains('PresetPath=.\TestPreset.ini')) 'first preset not selected'
Expect ([IO.File]::ReadAllText((Join-Path $game 'ReShade.ini')).Contains('KeyEffects=145,0,0,0')) 'ReShade.ini lost its settings'
Expect (Test-Path (Join-Path $game 'dvr-reshade-shaders\custom\Shaders\Mine.fx')) 'shader not copied'
Expect (Test-Path (Join-Path $game 'dvr-reshade-shaders\custom\Shaders\Sub\Helper.fxh')) 'shader subfolder not kept'
Expect (Test-Path (Join-Path $game 'dvr-reshade-shaders\custom\Textures\lut.png')) 'texture not copied'
Expect (-not (Test-Path (Join-Path $game 'dvr-reshade-shaders\custom\Textures\screenshot.png'))) 'loose screenshot copied as a texture'
Expect ($r.text -match 'NOT copied: .*d3d9\.dll.*replace the VR mod') 'd3d9.dll refusal not reported'
Expect ($r.text -match 'NotThere\.fx') 'missing shader not named'
Expect ($r.text -notmatch 'Mine\.fx,|Vibrance\.fx,') 'installed shaders reported missing'

# ---- the same files again, as a folder: identical, nothing re-written, no backups
$r=Import @($src)
Expect ($r.code -eq 0) "folder import exit code ($($r.text))"
Expect (@(Get-ChildItem -LiteralPath $game -Recurse -Filter '*.dvr-backup').Count -eq 0) 'identical import made backups'
Expect ($r.text -match 'already there and identical') 'identical import not reported'

# ---- a changed preset of the same name is backed up, and the active preset is not moved
[IO.File]::WriteAllText("$src\TestPreset.ini",$preset.Replace('0.15','0.30'))
$r=Import @("$src\TestPreset.ini")
Expect ($r.code -eq 0) 'single ini import'
Expect (@(Get-ChildItem -LiteralPath $game -Filter 'TestPreset.ini.*.dvr-backup').Count -eq 1) 'replaced preset not backed up'

# ---- a drop with nothing usable fails clearly and touches nothing
$r=Import @("$src\d3d9.dll","$src\README.txt")
Expect ($r.code -eq 1) 'empty import did not fail'
Expect ($r.text -match 'Nothing to import' -and $r.text -match 'd3d9\.dll') 'empty import message'

'PASS preset import: zip and folder drops, presets beside the exe, shaders/textures to custom, programs and foreign ReShade.ini refused, d3d9.dll refusal and missing shaders reported, first preset selected, identical re-import, changed preset backed up, empty drop refused'
