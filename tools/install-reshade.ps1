# Optional launcher operation. Download from upstream; never execute the setup EXE.
param([Parameter(Mandatory=$true)][string]$GameDir, [string]$DownloadFile = '')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
try {
    $targetDir = (Resolve-Path -LiteralPath $GameDir).Path
    if (-not (Test-Path -LiteralPath (Join-Path $targetDir 'Dishonored.exe'))) { throw 'Choose the folder containing Dishonored.exe.' }
    if (Get-Process -Name Dishonored -ErrorAction SilentlyContinue) { throw 'Close Dishonored before installing ReShade.' }
    $url = 'https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe'
    $expected = 'AFE4C8F13048306307983B8B3D41D5BF00A86820440B0E57DEA10950E1176445'
    if (-not $DownloadFile) {
        $DownloadFile = Join-Path $PSScriptRoot 'ReShade_Setup_6.8.0_Addon.exe'
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $DownloadFile -TimeoutSec 120
    }
    if ((Get-FileHash -LiteralPath $DownloadFile -Algorithm SHA256).Hash -ne $expected) { throw 'ReShade download hash does not match the pinned official 6.8.0 add-on build. No game files changed.' }
    $bytes = [IO.File]::ReadAllBytes($DownloadFile)
    $offset = -1
    for ($i=0; $i -lt $bytes.Length-4; $i+=512) {
        if ($bytes[$i] -eq 80 -and $bytes[$i+1] -eq 75 -and $bytes[$i+2] -eq 3 -and $bytes[$i+3] -eq 4) { $offset=$i; break }
    }
    if ($offset -lt 0) { throw 'Official installer ZIP payload not found.' }
    Add-Type -AssemblyName System.IO.Compression
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
    $staged=Join-Path $targetDir ('ReShade32.dll.'+[Guid]::NewGuid().ToString('N')+'.tmp')
    [IO.File]::WriteAllBytes($staged,$runtime)
    try {
        if (Test-Path -LiteralPath $target) {
            $backup=$target+'.'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff')+'.dvr-backup'
            [IO.File]::Replace($staged,$target,$backup)
        } else { [IO.File]::Move($staged,$target) }
    } finally { if (Test-Path -LiteralPath $staged) { Remove-Item -LiteralPath $staged } }
    Write-Output 'Installed ReShade 6.8.0 full add-on runtime. Existing presets and d3d9.dll are unchanged. Add shader packages separately, then press Home in game to choose effects. Rename ReShade32.dll to ReShade32.dll.disabled to disable it.'
    exit 0
} catch { Write-Output ('ReShade installation failed: '+$_.Exception.Message); exit 1 }
