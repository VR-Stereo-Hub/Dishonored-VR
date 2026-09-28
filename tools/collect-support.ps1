# Local-only diagnostic bundle. No uploads or game asset collection.
# VR-215: newest-first history, streaming snapshots and a hard 24 MB ZIP budget.
param(
    [string]$GameDir = $PSScriptRoot,
    [string]$DataDir,
    [string]$OutDir,
    [switch]$IncludeLatestDump,
    [switch]$NoOpen,
    [ValidateRange(1048576,24000000)][long]$MaxZipBytes = 24000000
)
$ErrorActionPreference = 'Stop'
$env:PSModulePath = (Join-Path $PSHOME 'Modules') + ';' + $env:PSModulePath
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$utf8=New-Object Text.UTF8Encoding $false
[Console]::OutputEncoding=$utf8
function File-Sha256([string]$path) {
    $stream=$null; $sha=[Security.Cryptography.SHA256]::Create()
    try {
        $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
        return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')
    } finally { if($stream){$stream.Dispose()}; $sha.Dispose() }
}
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
if(-not [IO.Directory]::Exists($GameDir)) {throw 'Choose the game installation folder.'}
$iniPath=Join-Path $GameDir 'dishonored_vr.ini'
$ini=''
if([IO.File]::Exists($iniPath)) {try {$ini=[IO.File]::ReadAllText($iniPath)}catch{}}
if (-not $DataDir) {
    $DataDir = Join-Path $env:LOCALAPPDATA 'DishonoredVR'
    if ($env:DVR_DATA_DIR) { $DataDir = $env:DVR_DATA_DIR }
    $section = ''
    foreach ($line in ($ini -split '\r?\n')) {
        if ($line -match '^\s*\[([^]]+)\]') { $section = $Matches[1] }
        elseif ($section -eq 'Paths' -and $line -match '^\s*DataDir\s*=\s*(.+?)\s*$') { $DataDir = $Matches[1] }
    }
}
$stamp=(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+$PID
$candidates=@($OutDir)
if(-not $OutDir) {
    $candidates=@()
    $desktop=[Environment]::GetFolderPath('Desktop')
    if($desktop){$candidates+=Join-Path $desktop 'DishonoredVR Support'}
    $candidates+=Join-Path $env:LOCALAPPDATA 'DishonoredVR\Support'
    $candidates+=Join-Path ([IO.Path]::GetTempPath()) 'DishonoredVR Support'
}
$stage=$null; $outputErrors=@()
foreach($candidate in $candidates) {
    try {
        $candidate=[IO.Path]::GetFullPath($candidate)
        $attempt=Join-Path $candidate "support-$stamp"
        [void][IO.Directory]::CreateDirectory($attempt)
        $probe=Join-Path $attempt 'write-test.tmp';[IO.File]::WriteAllText($probe,'');[IO.File]::Delete($probe)
        $stage=$attempt;break
    } catch {$outputErrors+=($_.Exception.Message)}
}
if(-not $stage){throw ('Could not create a support output folder: '+($outputErrors -join '; '))}
$report = [ordered]@{ createdUtc=[DateTime]::UtcNow.ToString('o'); gameDir=$GameDir; dataDir=$DataDir; maxZipBytes=$MaxZipBytes; files=@(); omitted=@(); errors=@($outputErrors); dumpsIncluded=$false }
# VR-223: the headset the player reported in the launcher (recorded only).
$report.headset='not recorded'
try {
    $launcherIni=Join-Path $env:LOCALAPPDATA 'DishonoredVR\launcher.ini'
    if([IO.File]::Exists($launcherIni)) {
        $section=''
        foreach($line in [IO.File]::ReadAllLines($launcherIni)) {
            if($line -match '^\s*\[([^]]+)\]'){$section=$Matches[1]}
            elseif($section -eq 'Headset' -and $line -match '^\s*Model\s*=\s*(.+?)\s*$'){$report.headset=$Matches[1]}
        }
    }
} catch {$report.errors+=('headset : '+$_.Exception.Message)}
$queue=New-Object 'System.Collections.Generic.List[object]'
$seen=@{}
function Copy-Evidence([string]$source,[string]$name,[int]$priority=0,[bool]$excerpt=$false) {
    try {
        if(-not [IO.File]::Exists($source)){return}
        $source=[IO.Path]::GetFullPath($source)
        if($seen.ContainsKey($source)){return};$seen[$source]=$true
        $queue.Add([pscustomobject]@{source=$source;name=$name;priority=$priority;excerpt=$excerpt;modified=[IO.File]::GetLastWriteTimeUtc($source)})
    } catch {$report.errors+=("$name : "+$_.Exception.Message)}
}
function Add-History([string]$root,[string]$prefix,[string]$base,[int]$priority) {
    # Explicit allowlist: never scoop up arbitrary files from the game directory.
    $names=@("$base.log","$base.prev.log")+@(2..9 | ForEach-Object {"$base.prev$_.log"})
    foreach($name in $names){Copy-Evidence ([IO.Path]::Combine($root,$name)) "$prefix$name" $priority ($name -eq "$base.log")}
}
# Small diagnostic context has a bounded reservation before the game history.
foreach($name in @('dishonored_vr.ini','dishonored_vr_crash.txt')) {Copy-Evidence (Join-Path $GameDir $name) $name 0 $true}
Copy-Evidence (Join-Path $GameDir 'dishonored_vr_install.json') 'install-record.json' 0 $true
$localEvidence=Join-Path $env:LOCALAPPDATA 'DishonoredVR'
Copy-Evidence (Join-Path $localEvidence 'dishonored_vr_launcher.log') 'local-dishonored_vr_launcher.log' 0 $true
Add-History $GameDir '' 'dishonored_vr' 1
foreach($root in @($localEvidence,$DataDir) | Select-Object -Unique) {
    $prefix=if($root -eq $localEvidence){'local-'}else{'data-'}
    foreach($base in @('dishonored_vr_launcher','dishonored_vr_setup','dishonored_vr')){Add-History $root $prefix $base 2}
    Copy-Evidence ([IO.Path]::Combine($root,'ovrshim.log')) ($prefix+'ovrshim.log') 2 $true
}
Copy-Evidence (Join-Path $GameDir 'ovrshim.log') 'game-ovrshim.log' 2 $true
foreach($name in @('actions.json','bindings_knuckles.json','bindings_vive_controller.json','bindings_oculus_touch.json','bindings_holographic_controller.json')) {
    Copy-Evidence (Join-Path (Join-Path $GameDir 'openvr_input') $name) "steamvr-$name" 0
}
if ($GameDir -match '^[A-Za-z]:\\') {
    $shadow=Join-Path (Join-Path $env:LOCALAPPDATA 'VirtualStore') $GameDir.Substring(3)
    Copy-Evidence (Join-Path $shadow 'dishonored_vr.ini') 'virtualstore-dishonored_vr.ini' 0 $true
    Add-History $shadow 'virtualstore-' 'dishonored_vr' 2
}
$gameConfig=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Dishonored\DishonoredGame\Config'
foreach($name in @('DishonoredEngine.ini','DishonoredInput.ini')) {Copy-Evidence (Join-Path $gameConfig $name) "game-$name" 0 $true}
try {
    $os=Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $cpu=Get-CimInstance Win32_Processor -ErrorAction Stop
    $gpu=Get-CimInstance Win32_VideoController -ErrorAction Stop
    $report.machine=@{os=$os.Caption;build=$os.BuildNumber;ramGB=[Math]::Round($os.TotalVisibleMemorySize / 1MB,1);cpu=@($cpu.Name);gpu=@($gpu | Select-Object Name,DriverVersion)}
} catch {$report.errors+=('Machine details unavailable: '+$_.Exception.Message)}
$runtimeKey=$null; $runtimeBase=$null
try {
    $runtimeBase=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::Registry32)
    $runtimeKey=$runtimeBase.OpenSubKey('SOFTWARE\Khronos\OpenXR\1')
    $report.activeRuntime32=if($runtimeKey){$runtimeKey.GetValue('ActiveRuntime','')}else{''}
} catch {$report.errors+=('32-bit runtime unavailable: '+$_.Exception.Message)}
finally {if($runtimeKey){$runtimeKey.Dispose()};if($runtimeBase){$runtimeBase.Dispose()}}
foreach($name in @('status.json','ovrshim.log','pacetrace.log')){Copy-Evidence ([IO.Path]::Combine($DataDir,$name)) $name 2 $true}
# Bounded watchdog extract from the latest 4 MB, never materialize a whole trace.
$trace=[IO.Path]::Combine($DataDir,'pacetrace.log')
if([IO.File]::Exists($trace)) {
    $stream=$null
    try {
        $stream=[IO.File]::Open($trace,'Open','Read',([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        $length=$stream.Length;$count=[int][Math]::Min($length,4MB);$buffer=New-Object byte[] $count
        [void]$stream.Seek($length-$count,'Begin');$done=0
        while($done -lt $count){$n=$stream.Read($buffer,$done,$count-$done);if(!$n){break};$done+=$n}
        $wd=@(([Text.Encoding]::UTF8.GetString($buffer,0,$done) -split '\r?\n') | Where-Object {$_ -match 'WATCHDOG'} | Select-Object -Last 500)
        $extract=Join-Path $stage 'pacetrace-watchdog.txt'
        $lines=@("Extract from last $done bytes of $length bytes; at most 500 WATCHDOG lines.")+$wd
        if(!$wd.Count){$lines+='No WATCHDOG lines in the inspected tail.'}
        [IO.File]::WriteAllLines($extract,$lines,$utf8)
        Copy-Evidence $extract 'pacetrace-watchdog.txt' 0 $true
    } catch {$report.errors+=('Watchdog extract: '+$_.Exception.Message)}
    finally {if($stream){$stream.Dispose()}}
}
$dumpDir=[IO.Path]::Combine($DataDir,'dumps')
$dumps=@()
if([IO.Directory]::Exists($dumpDir)){$dumps=@(Get-ChildItem -LiteralPath $dumpDir -Filter '*.dmp' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending)}
$report.dumpInventory=@($dumps | Select-Object -First 20 Name,Length,LastWriteTimeUtc)
if($IncludeLatestDump -and $dumps.Count){Copy-Evidence $dumps[0].FullName $dumps[0].Name 3}
$report.binaries=@()
foreach($name in @('d3d9.dll','Dishonored.exe','dvr_steamvr32.dll','openvr_api.dll')) {
    $file=Join-Path $GameDir $name
    if([IO.File]::Exists($file)) {
        try {$report.binaries+=@{name=$name;sha256=(File-Sha256 $file);version=(Get-Item -LiteralPath $file).VersionInfo.FileVersion}}
        catch {$report.errors+=("Binary $name : "+$_.Exception.Message)}
    }
}
# Compress each snapshot separately first. The sum of these complete ZIP sizes is
# an upper bound on the final archive (which needs only one end-of-directory).
# This avoids copying huge raw logs, loading them into RAM or guessing a ratio.
function Compress-Evidence($item,[long]$limit,[long]$excerptBytes=0) {
    $part=Join-Path $stage ([Guid]::NewGuid().ToString('N')+'.part.zip')
    $inputStream=$null;$outputStream=$null;$archive=$null;$entryStream=$null
    $hash=[Security.Cryptography.SHA256]::Create();$bytesWritten=0L;$tooLarge=$false;$failure=$null;$originalLength=0L
    try {
        $inputStream=[IO.File]::Open($item.source,'Open','Read',([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        $originalLength=$inputStream.Length
        $outputStream=[IO.File]::Create($part)
        $archive=[IO.Compression.ZipArchive]::new($outputStream,[IO.Compression.ZipArchiveMode]::Create,$true)
        $entryStream=$archive.CreateEntry($item.name,[IO.Compression.CompressionLevel]::Optimal).Open()
        $ranges=@([pscustomobject]@{offset=0L;count=$originalLength})
        if($excerptBytes -gt 0 -and $originalLength -gt $excerptBytes) {
            $head=[long][Math]::Min(65536,[Math]::Floor($excerptBytes/4))
            $tail=$excerptBytes-$head
            $ranges=@([pscustomobject]@{offset=0L;count=$head},[pscustomobject]@{offset=($originalLength-$tail);count=$tail})
        }
        $buffer=New-Object byte[] 65536
        for($rangeIndex=0;$rangeIndex -lt $ranges.Count;$rangeIndex++) {
            if($rangeIndex -gt 0) {
                $marker=$utf8.GetBytes("`r`n[SUPPORT EXCERPT: middle omitted to fit ZIP budget; original $originalLength bytes]`r`n")
                $entryStream.Write($marker,0,$marker.Length);[void]$hash.TransformBlock($marker,0,$marker.Length,$null,0);$bytesWritten+=$marker.Length
            }
            [void]$inputStream.Seek($ranges[$rangeIndex].offset,'Begin');$left=[long]$ranges[$rangeIndex].count
            while($left -gt 0) {
                $n=$inputStream.Read($buffer,0,[int][Math]::Min($left,$buffer.Length))
                if(!$n){break}
                $entryStream.Write($buffer,0,$n);[void]$hash.TransformBlock($buffer,0,$n,$null,0)
                $bytesWritten+=$n;$left-=$n
                if($outputStream.Length -gt $limit){$tooLarge=$true;break}
            }
            if($tooLarge){break}
        }
        [void]$hash.TransformFinalBlock([byte[]]@(),0,0)
        $digest=[BitConverter]::ToString($hash.Hash).Replace('-','')
    } catch {$failure=$_.Exception.Message}
    finally {
        if($entryStream){$entryStream.Dispose()};if($archive){$archive.Dispose()};if($outputStream){$outputStream.Dispose()};if($inputStream){$inputStream.Dispose()};$hash.Dispose()
    }
    if($failure -or $tooLarge -or ([IO.FileInfo]$part).Length -gt $limit) {
        [IO.File]::Delete($part)
        return [pscustomobject]@{ok=$false;error=$failure;sizeLimited=(!$failure)}
    }
    return [pscustomobject]@{ok=$true;path=$part;size=([IO.FileInfo]$part).Length;name=$item.name;sha256=$digest;sourceBytes=$originalLength;copiedBytes=$bytesWritten;truncated=($ranges.Count -gt 1 -or $bytesWritten -lt $originalLength);sourceModifiedUtc=$item.modified.ToString('o')}
}
$accepted=New-Object 'System.Collections.Generic.List[object]'
# Metadata always fits in the reserved 128 KiB; small-context files share 512 KiB.
$remaining=$MaxZipBytes-131072;$contextRemaining=524288L
foreach($item in @($queue | Sort-Object priority,@{Expression='modified';Descending=$true},name)) {
    $limit=$remaining
    if($item.priority -eq 0){$limit=[Math]::Min($limit,[Math]::Min($contextRemaining,131072))}
    if($limit -lt 4096){$report.omitted+=@{name=$item.name;reason='ZIP budget exhausted';sourceModifiedUtc=$item.modified.ToString('o')};continue}
    $result=Compress-Evidence $item $limit
    if(!$result.ok -and $result.sizeLimited -and $item.excerpt) {
        # Current logs retain both the startup/build banner and the latest events.
        $result=Compress-Evidence $item $limit ([long][Math]::Floor($limit*0.99)-2048)
    }
    if(!$result.ok) {
        $reason=if($result.sizeLimited){'Does not fit remaining ZIP budget'}else{$result.error}
        $report.omitted+=@{name=$item.name;reason=$reason;sourceModifiedUtc=$item.modified.ToString('o')}
        if($result.error){$report.errors+=("$($item.name) : "+$result.error)}
        continue
    }
    $accepted.Add($result);$remaining-=$result.size
    if($item.priority -eq 0){$contextRemaining-=$result.size}
    $report.files+=@{name=$result.name;sha256=$result.sha256;sourceBytes=$result.sourceBytes;copiedBytes=$result.copiedBytes;truncated=$result.truncated;sourceModifiedUtc=$result.sourceModifiedUtc}
    if($item.name -like '*.dmp'){$report.dumpsIncluded=$true}
}
$report.order='Small context first, game logs newest to oldest, then other recent diagnostics. Full files preferred; current oversized logs retain header and tail.'
$manifest=$utf8.GetBytes(($report | ConvertTo-Json -Depth 8))
$readme=$utf8.GetBytes((@('Local diagnostic collection. Nothing was uploaded.',
 'Logs/settings may contain local paths and account names. Review before sharing.',
 'Dumps are opt-in and should be shared privately.',
 'manifest.json lists source times, hashes, omitted files and shortened excerpts.',
 'The ZIP stays below 24,000,000 bytes. Game logs are prioritized newest to oldest.',
 'Excerpts preserve startup/build information and the latest events; their middle is marked omitted.',
 'Describe what happened, texture mods, headset/runtime and whether it repeats.') -join "`r`n"))
$zip="$stage.zip";$pending="$zip.pending";$outputStream=$null;$archive=$null
try {
    $outputStream=[IO.File]::Create($pending)
    $archive=[IO.Compression.ZipArchive]::new($outputStream,[IO.Compression.ZipArchiveMode]::Create,$true)
    foreach($part in $accepted) {
        $sourceZip=[IO.Compression.ZipFile]::OpenRead($part.path);$sourceStream=$null;$destination=$null
        try {
            $sourceStream=$sourceZip.Entries[0].Open();$destination=$archive.CreateEntry($part.name,[IO.Compression.CompressionLevel]::Optimal).Open()
            $sourceStream.CopyTo($destination)
        } finally {if($destination){$destination.Dispose()};if($sourceStream){$sourceStream.Dispose()};$sourceZip.Dispose()}
    }
    foreach($metadata in @(@('manifest.json',$manifest),@('READ-ME.txt',$readme))) {
        $entry=$archive.CreateEntry($metadata[0],[IO.Compression.CompressionLevel]::Optimal).Open()
        try {$entry.Write($metadata[1],0,$metadata[1].Length)} finally {$entry.Dispose()}
    }
} finally {if($archive){$archive.Dispose()};if($outputStream){$outputStream.Dispose()}}
if(([IO.FileInfo]$pending).Length -gt $MaxZipBytes){[IO.File]::Delete($pending);throw 'Support ZIP exceeded its limit; no oversized ZIP was produced.'}
[IO.File]::Move($pending,$zip)
# Only known files produced by this invocation are removed; retain manifest beside ZIP.
foreach($part in $accepted){[IO.File]::Delete($part.path)}
[IO.File]::WriteAllBytes((Join-Path $stage 'manifest.json'),$manifest)
Write-Output "Support ZIP: $zip"
Write-Output "ZIP bytes: $(([IO.FileInfo]$zip).Length); files: $($report.files.Count); omitted: $($report.omitted.Count)"
if(-not $NoOpen) {try{Start-Process explorer.exe -ArgumentList ('/select,"'+$zip+'"')}catch{Write-Output 'The ZIP was created, but Explorer could not open it.'}}
