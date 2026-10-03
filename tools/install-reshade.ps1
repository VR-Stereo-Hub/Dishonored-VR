# Optional launcher operation. Download from upstream; never execute the setup EXE.
#
# Installs a ReShade that actually starts, not just its DLL:
#   1. ReShade32.dll from the pinned official 6.8.0 add-on setup (hash-checked, PE32 x86).
#   2. The three pinned shader packages the bundled preset support needs (Standard Effects,
#      SweetFX, prod80) into Win32\dvr-reshade-shaders\<name>, hash-checked, never over an
#      existing folder.
#   3. Win32\dvr-reshade-shaders\custom\Shaders and \Textures for the player's own shaders.
#   4. ReShade.ini beside the exe, ONLY when there is none. ReShade's DllMain refuses to load
#      (LoadLibrary error 1114) when no ReShade.ini exists for the exe and it is not loaded
#      under a proxy name, so a runtime installed without one never starts - the 1.0.3 fault.
#      An existing ReShade.ini is the player's and is never rewritten.
#
# -DownloadFile and -PackageDir let tests run offline from local copies of the same files.
param([Parameter(Mandatory=$true)][string]$GameDir, [string]$DownloadFile = '', [string]$PackageDir = '')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Get-Sha256([byte[]]$bytes) {
    # A launcher may inherit a PowerShell Core module path in Windows PowerShell.
    # Use .NET directly so verification does not depend on Get-FileHash discovery.
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha256.ComputeHash($bytes)).Replace('-', '') }
    finally { $sha256.Dispose() }
}

function Get-Download([string]$url, [string]$localName) {
    if ($PackageDir) { return [IO.File]::ReadAllBytes((Join-Path $PackageDir $localName)) }
    $file = Join-Path $PSScriptRoot $localName
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $file -TimeoutSec 120
    return [IO.File]::ReadAllBytes($file)
}

# Extract a GitHub archive into $dest, dropping its single top-level folder. Staged beside
# the destination and moved into place, so a failure never leaves a half-written package.
function Expand-Package([byte[]]$bytes, [string]$dest) {
    $staged = $dest + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    $stream = New-Object IO.MemoryStream(,$bytes)
    $zip = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Read)
    try {
        New-Item -ItemType Directory -Path $staged | Out-Null
        $root = [IO.Path]::GetFullPath($staged + '\')
        foreach ($entry in $zip.Entries) {
            $slash = $entry.FullName.IndexOf('/')
            if ($slash -lt 0) { continue }
            $rel = $entry.FullName.Substring($slash + 1)
            if (-not $rel) { continue }
            $out = [IO.Path]::GetFullPath((Join-Path $staged $rel))
            if (-not $out.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw "Package entry escapes its folder: $($entry.FullName)" }
            if ($entry.FullName.EndsWith('/')) { New-Item -ItemType Directory -Force -Path $out | Out-Null; continue }
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $out) | Out-Null
            $in = $entry.Open(); $fs = [IO.File]::Create($out)
            try { $in.CopyTo($fs) } finally { $fs.Dispose(); $in.Dispose() }
        }
        [IO.Directory]::Move($staged, $dest)
    } catch {
        if (Test-Path -LiteralPath $staged) { Remove-Item -LiteralPath $staged -Recurse -Force }
        throw
    } finally { $zip.Dispose(); $stream.Dispose() }
}

$report = New-Object Collections.Generic.List[string]
try {
    $targetDir = (Resolve-Path -LiteralPath $GameDir).Path
    if (-not (Test-Path -LiteralPath (Join-Path $targetDir 'Dishonored.exe'))) { throw 'Choose the folder containing Dishonored.exe.' }
    if (Get-Process -Name Dishonored -ErrorAction SilentlyContinue) { throw 'Close Dishonored before installing ReShade.' }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Add-Type -AssemblyName System.IO.Compression

    # ---- 1. the runtime -----------------------------------------------------------------
    $url = 'https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe'
    $expected = 'AFE4C8F13048306307983B8B3D41D5BF00A86820440B0E57DEA10950E1176445'
    if (-not $DownloadFile) {
        $DownloadFile = Join-Path $PSScriptRoot 'ReShade_Setup_6.8.0_Addon.exe'
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $DownloadFile -TimeoutSec 120
    }
    $bytes = [IO.File]::ReadAllBytes($DownloadFile)
    if ((Get-Sha256 $bytes) -ne $expected) { throw 'ReShade download hash does not match the pinned official 6.8.0 add-on build. No game files changed.' }
    $offset = -1
    for ($i=0; $i -lt $bytes.Length-4; $i+=512) {
        if ($bytes[$i] -eq 80 -and $bytes[$i+1] -eq 75 -and $bytes[$i+2] -eq 3 -and $bytes[$i+3] -eq 4) { $offset=$i; break }
    }
    if ($offset -lt 0) { throw 'Official installer ZIP payload not found.' }
    $stream = New-Object IO.MemoryStream(,$bytes[$offset..($bytes.Length-1)])
    $zip = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Read)
    try {
        $entry = $zip.GetEntry('ReShade32.dll')
        if (-not $entry -or $entry.Length -gt 64MB) { throw 'Official x86 runtime missing or too large.' }
        $inputStream=$entry.Open(); $outputStream=New-Object IO.MemoryStream
        try { $inputStream.CopyTo($outputStream); $runtime=$outputStream.ToArray() } finally { $inputStream.Dispose(); $outputStream.Dispose() }
    } finally { $zip.Dispose(); $stream.Dispose() }
    if ($runtime.Length -lt 256 -or $runtime[0] -ne 77 -or $runtime[1] -ne 90) { throw 'Runtime is not a PE image.' }
    $pe=[BitConverter]::ToInt32($runtime,60)
    if ($pe -lt 64 -or $pe+26 -ge $runtime.Length -or [BitConverter]::ToUInt32($runtime,$pe) -ne 17744 -or [BitConverter]::ToUInt16($runtime,$pe+4) -ne 332 -or [BitConverter]::ToUInt16($runtime,$pe+24) -ne 267) { throw 'Runtime is not PE32 x86.' }
    # Exact file whitelist: no ZIP paths or setup executable enter the game directory.
    $target=Join-Path $targetDir 'ReShade32.dll'
    if ((Test-Path -LiteralPath $target) -and (Get-Sha256 ([IO.File]::ReadAllBytes($target))) -eq (Get-Sha256 $runtime)) {
        # Re-running the install must not pile up identical backups.
        $report.Add('ReShade 6.8.0 runtime already installed.')
    } else {
        $staged=Join-Path $targetDir ('ReShade32.dll.'+[Guid]::NewGuid().ToString('N')+'.tmp')
        [IO.File]::WriteAllBytes($staged,$runtime)
        try {
            if (Test-Path -LiteralPath $target) {
                $backup=$target+'.'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff')+'.dvr-backup'
                [IO.File]::Replace($staged,$target,$backup)
            } else { [IO.File]::Move($staged,$target) }
        } finally { if (Test-Path -LiteralPath $staged) { Remove-Item -LiteralPath $staged } }
        $report.Add('Installed ReShade 6.8.0 full add-on runtime. d3d9.dll is unchanged.')
    }

    # ---- 2. shader packages ---------------------------------------------------------------
    # Pinned commits and download hashes: docs/INSTALLER.md (ReShade preset dependencies).
    $packages = @(
        @{ Name='standard'; File='standard.zip'; Sha='A3B110BA5118F3B944D74F0B0746C21280071D389ED98D615BF3C4B3A1778586'
           Url='https://codeload.github.com/crosire/reshade-shaders/zip/fd0022170615ce0d8162d219bff07232fa6dd84f' },
        @{ Name='sweetfx'; File='sweetfx.zip'; Sha='E1E1D6515D29C65FCF115C9692A1C5F91FFB8D9734E6871BCF67AC48588DBBCE'
           Url='https://codeload.github.com/CeeJayDK/SweetFX/zip/93ddf39b357f5da534ed6d34ba4ec8cc7dcfa361' },
        @{ Name='prod80'; File='prod80.zip'; Sha='15B251A3F99901DDA81072C3CB8FFA1EB2144DEE5399D459A5DDE7A50BBD6132'
           Url='https://codeload.github.com/prod80/prod80-ReShade-Repository/zip/1c2ed5b093b03c558bfa6aea45c2087052e99554' }
    )
    $shaderRoot = Join-Path $targetDir 'dvr-reshade-shaders'
    New-Item -ItemType Directory -Force -Path $shaderRoot | Out-Null
    $failed = New-Object Collections.Generic.List[string]
    foreach ($p in $packages) {
        $dest = Join-Path $shaderRoot $p.Name
        if (Test-Path -LiteralPath $dest) { $report.Add("Shader package $($p.Name): already present, kept."); continue }
        try {
            $zipBytes = Get-Download $p.Url $p.File
            if ((Get-Sha256 $zipBytes) -ne $p.Sha) { throw 'download hash does not match the pinned package' }
            Expand-Package $zipBytes $dest
            $report.Add("Shader package $($p.Name): installed.")
        } catch { $failed.Add("$($p.Name) ($($_.Exception.Message))") }
    }
    foreach ($sub in 'Shaders','Textures') { New-Item -ItemType Directory -Force -Path (Join-Path $shaderRoot "custom\$sub") | Out-Null }

    # ---- 3. ReShade.ini, only when there is none ------------------------------------------
    $ini = Join-Path $targetDir 'ReShade.ini'
    if (Test-Path -LiteralPath $ini) {
        $report.Add('Kept your existing ReShade.ini unchanged (its shader search paths are yours).')
    } else {
        $effects  = '.\dvr-reshade-shaders\standard\Shaders,.\dvr-reshade-shaders\sweetfx\Shaders\SweetFX,.\dvr-reshade-shaders\prod80\Shaders,.\dvr-reshade-shaders\custom\Shaders\**'
        $textures = '.\dvr-reshade-shaders\standard\Textures,.\dvr-reshade-shaders\sweetfx\Textures\SweetFX,.\dvr-reshade-shaders\prod80\Textures,.\dvr-reshade-shaders\custom\Textures\**'
        $lines = @('[GENERAL]', "EffectSearchPaths=$effects", "TextureSearchPaths=$textures")
        $carinth = Join-Path $targetDir 'DishonoredCarinthPresetv3.ini'
        if (Test-Path -LiteralPath $carinth) { $lines += 'PresetPath=.\DishonoredCarinthPresetv3.ini' }
        $lines += @('PerformanceMode=1', 'SkipLoadingDisabledEffects=1', '', '[INPUT]', 'KeyEffects=145,0,0,0', 'KeyOverlay=36,0,0,0', '')
        [IO.File]::WriteAllText($ini, ($lines -join "`r`n"), (New-Object Text.UTF8Encoding $false))
        $report.Add('Created ReShade.ini (ReShade will not start without one).')
    }
    $report.Add('Your own shaders: dvr-reshade-shaders\custom\Shaders and \Textures. Presets: drop a preset download on the launcher''s Mods screen (only the preset is taken from it), then pick it in F10 > ReShade.')
    $report.Add('Next: click Turn ReShade on (or use F10 > ReShade in game), then start the game. ReShade stays off until you turn it on.')
    if ($failed.Count) {
        Write-Output ('ReShade runtime installed, but these shader packages could not be installed: ' + ($failed -join '; ') + '. Check the internet connection and click Install ReShade again; installed parts are kept. | ' + ($report -join ' | '))
        exit 1
    }
    Write-Output ($report -join ' | ')
    exit 0
} catch { Write-Output ('ReShade installation failed: '+$_.Exception.Message); exit 1 }
