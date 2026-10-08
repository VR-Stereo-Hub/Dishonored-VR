# Launcher operation: import ReShade presets (and their shaders) dropped on the launcher.
#
# Accepts any mix of files, folders and .zip archives, listed one path per line in
# -ListFile (UTF-8). Copies ONLY:
#   - ReShade preset .ini files (a line `Techniques=`), beside Dishonored.exe;
#   - shader sources (.fx/.fxh) into Win32\dvr-reshade-shaders\custom\Shaders, keeping
#     the folders below the archive's own `Shaders` folder so #includes still resolve;
#   - textures from a `Textures` folder into dvr-reshade-shaders\custom\Textures.
# Never copies a program: .dll/.exe/.asi/.addon* are refused by name, so a preset
# download's d3d9.dll (an old ReShade or ENB proxy) can never replace the VR mod.
# ReShade.ini and dishonored_vr.ini from a download are refused; the player's own are kept.
param([Parameter(Mandatory=$true)][string]$GameDir, [Parameter(Mandatory=$true)][string]$ListFile)
$ErrorActionPreference = 'Stop'

$programExt = '.dll','.exe','.asi','.addon','.addon32','.addon64','.bat','.cmd','.ps1'
$shaderExt  = '.fx','.fxh'
$textureExt = '.png','.jpg','.jpeg','.dds','.bmp','.tga'
$refusedIni = 'reshade.ini','dishonored_vr.ini','reshadepreset.ini.bak'

$report   = New-Object Collections.Generic.List[string]
$presets  = New-Object Collections.Generic.List[string]
$programs = New-Object 'Collections.Generic.SortedSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$shaders = 0; $textures = 0; $unchanged = 0; $other = 0

function Get-Sha([byte[]]$b) {
    $s = [Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($s.ComputeHash($b)) } finally { $s.Dispose() }
}

# Write $bytes to $dest. Identical content is left alone; a different file is kept as a
# .dvr-backup first, so an import can always be undone by hand.
function Write-Item([byte[]]$bytes, [string]$dest) {
    if (Test-Path -LiteralPath $dest) {
        if ((Get-Sha ([IO.File]::ReadAllBytes($dest))) -eq (Get-Sha $bytes)) { $script:unchanged++; return $false }
        Copy-Item -LiteralPath $dest -Destination ($dest + '.' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '.dvr-backup')
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
    [IO.File]::WriteAllBytes($dest, $bytes)
    return $true
}

# Path components below the LAST folder named $anchor, or $null when there is none.
function Get-Below([string[]]$parts, [string]$anchor) {
    for ($i = $parts.Length - 2; $i -ge 0; $i--) {
        if ($parts[$i] -ieq $anchor) { return $parts[($i + 1)..($parts.Length - 1)] }
    }
    return $null
}

# One file, from disk or from an archive. $rel is its path inside what was dropped.
function Import-Item([string]$rel, [scriptblock]$read) {
    $parts = @($rel -split '[\\/]' | Where-Object { $_ -and $_ -ne '.' })
    if (-not $parts.Length -or ($parts -contains '..')) { return }
    $name = $parts[-1]; $ext = [IO.Path]::GetExtension($name).ToLowerInvariant()
    if ($programExt -contains $ext) { [void]$programs.Add($name); return }
    if ($ext -eq '.ini') {
        if ($refusedIni -contains $name.ToLowerInvariant()) { $script:other++; return }
        $bytes = & $read
        $text = [Text.Encoding]::UTF8.GetString($bytes)
        if ($text -notmatch '(?m)^\s*Techniques\s*=') { $script:other++; return }
        [void](Write-Item $bytes (Join-Path $targetDir $name))
        if (-not $presets.Contains($name)) { $presets.Add($name) }
        return
    }
    if ($shaderExt -contains $ext) {
        $below = Get-Below $parts 'Shaders'; if (-not $below) { $below = @($name) }
        if (Write-Item (& $read) (Join-Path $customShaders ($below -join '\'))) { $script:shaders++ }
        return
    }
    if ($textureExt -contains $ext) {
        # Only a Textures folder: loose images in a download are screenshots, not textures.
        $below = Get-Below $parts 'Textures'; if (-not $below) { $script:other++; return }
        if (Write-Item (& $read) (Join-Path $customTextures ($below -join '\'))) { $script:textures++ }
        return
    }
    $script:other++
}

function Import-Zip([string]$path) {
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($path)
    try {
        foreach ($e in $zip.Entries) {
            if ($e.FullName.EndsWith('/') -or $e.Length -gt 256MB) { continue }
            $entry = $e
            Import-Item $e.FullName { $s = $entry.Open(); $m = New-Object IO.MemoryStream
                                      try { $s.CopyTo($m); ,$m.ToArray() } finally { $s.Dispose(); $m.Dispose() } }.GetNewClosure()
        }
    } finally { $zip.Dispose() }
}

try {
    $targetDir = (Resolve-Path -LiteralPath $GameDir).Path
    if (-not (Test-Path -LiteralPath (Join-Path $targetDir 'Dishonored.exe'))) { throw 'The game folder was not found.' }
    $customShaders  = Join-Path $targetDir 'dvr-reshade-shaders\custom\Shaders'
    $customTextures = Join-Path $targetDir 'dvr-reshade-shaders\custom\Textures'
    $paths = @([IO.File]::ReadAllLines($ListFile, [Text.Encoding]::UTF8) | Where-Object { $_ })
    foreach ($p in $paths) {
        if (Test-Path -LiteralPath $p -PathType Container) {
            $root = (Resolve-Path -LiteralPath $p).Path.TrimEnd('\')
            $base = Split-Path -Leaf $root
            foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File) {
                $full = $f.FullName
                if ($f.Extension -ieq '.zip') { Import-Zip $full; continue }
                Import-Item ($base + '\' + $full.Substring($root.Length + 1)) { ,[IO.File]::ReadAllBytes($full) }.GetNewClosure()
            }
        } elseif ([IO.Path]::GetExtension($p) -ieq '.zip') {
            Import-Zip $p
        } elseif (Test-Path -LiteralPath $p -PathType Leaf) {
            $full = $p
            Import-Item (Split-Path -Leaf $p) { ,[IO.File]::ReadAllBytes($full) }.GetNewClosure()
        }
    }

    foreach ($n in $presets) { $report.Add("Preset ready: $n (pick it in F10 > ReShade).") }
    if ($shaders)  { $report.Add("$shaders shader file(s) copied to dvr-reshade-shaders\custom\Shaders.") }
    if ($textures) { $report.Add("$textures texture(s) copied to dvr-reshade-shaders\custom\Textures.") }
    if ($unchanged) { $report.Add("$unchanged file(s) were already there and identical.") }
    if ($programs.Count) {
        $report.Add('NOT copied: ' + ($programs -join ', ') + ' - presets never need a program file' +
            $(if ($programs.Contains('d3d9.dll')) { ', and a d3d9.dll would replace the VR mod' } else { '' }) + '.')
    }

    # Name the shaders each imported preset uses that no search path can find.
    $shaderRoot = Join-Path $targetDir 'dvr-reshade-shaders'
    $have = @{}
    if (Test-Path -LiteralPath $shaderRoot) { foreach ($f in Get-ChildItem -LiteralPath $shaderRoot -Recurse -File -Filter *.fx) { $have[$f.Name.ToLowerInvariant()] = $true } }
    foreach ($n in $presets) {
        $text = [IO.File]::ReadAllText((Join-Path $targetDir $n))
        $m = [regex]::Match($text, '(?m)^\s*Techniques\s*=(.*)$')
        $missing = New-Object Collections.Generic.SortedSet[string]
        foreach ($t in $m.Groups[1].Value.Split(',')) {
            $at = $t.IndexOf('@'); if ($at -lt 0) { continue }
            $fx = $t.Substring($at + 1).Trim()
            if ($fx -and -not $have.ContainsKey($fx.ToLowerInvariant())) { [void]$missing.Add($fx) }
        }
        if ($missing.Count) { $report.Add("$n uses shaders that are not installed: " + ($missing -join ', ') + '. Drop their shader package here too; those effects stay off until then.') }
    }

    # A first preset is selected for the player when ReShade.ini names none yet.
    $ini = Join-Path $targetDir 'ReShade.ini'
    if ($presets.Count -eq 1 -and (Test-Path -LiteralPath $ini)) {
        $lines = [Collections.Generic.List[string]]([IO.File]::ReadAllLines($ini))
        $current = $lines | Where-Object { $_ -match '^\s*PresetPath\s*=' } | Select-Object -First 1
        $currentFile = if ($current) { Join-Path $targetDir ($current.Split('=',2)[1].Trim()) } else { '' }
        $general = $lines.FindIndex([Predicate[string]]{ param($l) $l.Trim() -ieq '[GENERAL]' })
        if ((-not $current -or -not (Test-Path -LiteralPath $currentFile)) -and $general -ge 0) {
            if ($current) { [void]$lines.Remove($current) }
            $lines.Insert($general + 1, 'PresetPath=.\' + $presets[0])
            [IO.File]::WriteAllText($ini, (($lines -join "`r`n") + "`r`n"), (New-Object Text.UTF8Encoding $false))
            $report.Add("$($presets[0]) is now the active preset.")
        }
    }

    if (-not $presets.Count -and -not $shaders -and -not $textures -and -not $unchanged) {
        Write-Output ('Nothing to import: no ReShade preset (.ini with a Techniques line), shader (.fx/.fxh) or Textures folder was found.' + $(if ($report.Count) { ' | ' + ($report -join ' | ') } else { '' }))
        exit 1
    }
    Write-Output ($report -join ' | ')
    exit 0
} catch { Write-Output ('Preset import failed: ' + $_.Exception.Message); exit 1 }
