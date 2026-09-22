# Local-only diagnostic bundle. No uploads or game asset collection.
param(
    [string]$GameDir = $PSScriptRoot,
    [string]$DataDir,
    [string]$OutDir = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'DishonoredVR Support'),
    [switch]$IncludeLatestDump,
    [switch]$NoOpen
)
$ErrorActionPreference = 'Stop'
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
if (-not (Test-Path -LiteralPath (Join-Path $GameDir 'dishonored_vr.ini'))) { throw 'Choose the Win32 folder containing dishonored_vr.ini.' }
$ini = Get-Content -LiteralPath (Join-Path $GameDir 'dishonored_vr.ini') -Raw
if (-not $DataDir) {
    $DataDir = Join-Path $env:LOCALAPPDATA 'DishonoredVR'
    if ($env:DVR_DATA_DIR) { $DataDir = $env:DVR_DATA_DIR }
    $section = ''
    foreach ($line in ($ini -split '\r?\n')) {
        if ($line -match '^\s*\[([^]]+)\]') { $section = $Matches[1] }
        elseif ($section -eq 'Paths' -and $line -match '^\s*DataDir\s*=\s*(.+?)\s*$') { $DataDir = $Matches[1] }
    }
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$stage = Join-Path $OutDir "support-$stamp"
New-Item -ItemType Directory -Path $stage -Force | Out-Null
$report = [ordered]@{ createdUtc=[DateTime]::UtcNow.ToString('o'); gameDir=$GameDir; dataDir=$DataDir; files=@(); errors=@(); dumpsIncluded=[bool]$IncludeLatestDump }
function Copy-Evidence([string]$source,[string]$name) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { return }
    $inputStream=$null; $outputStream=$null
    try {
        $inputStream=[IO.File]::Open($source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
        $target=Join-Path $stage $name
        $outputStream=[IO.File]::Create($target)
        $inputStream.CopyTo($outputStream); $outputStream.Dispose(); $outputStream=$null
        $report.files+=@{ name=$name; sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash; sourceModifiedUtc=(Get-Item -LiteralPath $source).LastWriteTimeUtc.ToString('o') }
    } catch { $report.errors+=("$name : "+$_.Exception.Message) }
    finally { if($outputStream){$outputStream.Dispose()}; if($inputStream){$inputStream.Dispose()} }
}
foreach($name in @('dishonored_vr.log','dishonored_vr.prev.log','dishonored_vr.ini','dishonored_vr_crash.txt')) { Copy-Evidence (Join-Path $GameDir $name) $name }
# VR-177: pacetrace.log carries the runtime watchdog's stacks - the only freeze evidence there is.
foreach($name in @('status.json','ovrshim.log','pacetrace.log')) { Copy-Evidence (Join-Path $DataDir $name) $name }
# ...and the watchdog lines alone, so a freeze report is readable without the whole trace.
$trace=Join-Path $DataDir 'pacetrace.log'
if (Test-Path -LiteralPath $trace -PathType Leaf) {
    try {
        $wd=@(Select-String -LiteralPath $trace -Pattern 'WATCHDOG' -SimpleMatch | ForEach-Object { $_.Line })
        $wdOut=Join-Path $stage 'pacetrace-watchdog.txt'
        if ($wd.Count) { $wd | Set-Content -LiteralPath $wdOut -Encoding UTF8 }
        else { 'no WATCHDOG lines in pacetrace.log (no stall of 4 s or more was photographed)' | Set-Content -LiteralPath $wdOut -Encoding UTF8 }
        $report.files+=@{ name='pacetrace-watchdog.txt'; lines=$wd.Count }
    } catch { $report.errors+=('pacetrace-watchdog.txt : '+$_.Exception.Message) }
}
$dumpDir=Join-Path $DataDir 'dumps'
$dumps=@(Get-ChildItem -LiteralPath $dumpDir -Filter '*.dmp' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending)
$report.dumpInventory=@($dumps | Select-Object Name,Length,LastWriteTimeUtc)
if($IncludeLatestDump -and $dumps.Count) { Copy-Evidence $dumps[0].FullName $dumps[0].Name }
$report.binaries=@()
foreach($name in @('d3d9.dll','Dishonored.exe','dvr_steamvr32.dll','openvr_api.dll')) {
    $file=Join-Path $GameDir $name
    if(Test-Path -LiteralPath $file) { $report.binaries+=@{ name=$name; sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash; version=(Get-Item -LiteralPath $file).VersionInfo.FileVersion } }
}
$report | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $stage 'manifest.json') -Encoding UTF8
@'
Local diagnostic collection. Nothing was uploaded.
Logs/settings may contain local paths and account names. Review before sharing.
Dumps, when explicitly included, contain process memory and should be shared privately.
Timestamps and DLL hashes identify the run; old crash files/dumps may describe older runs.
Describe what you were doing, texture mods, headset/runtime, and whether this repeats.
'@ | Set-Content -LiteralPath (Join-Path $stage 'READ-ME.txt') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip="$stage.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip)
Write-Output "Support ZIP: $zip"
if(-not $NoOpen) { Start-Process explorer.exe -ArgumentList ('/select,"'+$zip+'"') }
