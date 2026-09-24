# VR-215: actual compressed-size limit, source fidelity and diagnostic fallbacks.
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('dvr-support-budget-'+[Guid]::NewGuid().ToString('N'))
$game=Join-Path $root 'game';$data=Join-Path $root 'data'
New-Item -ItemType Directory -Path $game,$data -Force | Out-Null
$ascii=[Text.Encoding]::ASCII
[IO.File]::WriteAllText((Join-Path $game 'dishonored_vr.ini'),"[Paths]`r`nDataDir=$data`r`n",$ascii)
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$checks=0
function Assert($value,$label){if(!$value){throw $label};$script:checks++;Write-Host "  ok   $label"}
function Random-File($path,[long]$length) {
 $rng=[Security.Cryptography.RandomNumberGenerator]::Create();$stream=[IO.File]::Create($path);$bytes=New-Object byte[] 65536
 try {while($length -gt 0){$rng.GetBytes($bytes);$count=[int][Math]::Min($length,$bytes.Length);$stream.Write($bytes,0,$count);$length-=$count}}finally{$stream.Dispose();$rng.Dispose()}
}
function Collect($label,[long]$budget=24000000) {
 & "$PSScriptRoot\collect-support.ps1" -GameDir $game -DataDir $data -OutDir (Join-Path $root $label) -MaxZipBytes $budget -NoOpen | Out-Host
 $zip=Get-ChildItem (Join-Path $root $label) -Filter '*.zip' | Select-Object -First 1
 Assert ($zip.Length -le $budget) "archive $label is below $budget bytes ($($zip.Length))"
 return [IO.Compression.ZipFile]::OpenRead($zip.FullName)
}
function Read-Entry($zip,$name) {
 $stream=$zip.GetEntry($name).Open();$memory=New-Object IO.MemoryStream
 try {$stream.CopyTo($memory);return ,$memory.ToArray()}finally{$stream.Dispose();$memory.Dispose()}
}
function Manifest($zip){$bytes=Read-Entry $zip 'manifest.json';return ([Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json)}
function Hash($bytes){$sha=[Security.Cryptography.SHA256]::Create();try{return [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','')}finally{$sha.Dispose()}}
# Deliberately incompressible logs: current fits, prior large file does not, older small file does.
$now=[DateTime]::UtcNow
$names=@('dishonored_vr.log','dishonored_vr.prev.log','dishonored_vr.prev2.log')
$sizes=@(600000,500000,100000)
for($i=0;$i -lt 3;$i++){Random-File (Join-Path $game $names[$i]) $sizes[$i];[IO.File]::SetLastWriteTimeUtc((Join-Path $game $names[$i]),$now.AddMinutes(-$i))}
$zip=Collect 'small-budget' 1048576
try {
 $manifest=Manifest $zip
 Assert ($null -ne $zip.GetEntry($names[0]) -and $null -ne $zip.GetEntry($names[2])) 'newest and fitting older logs included'
 Assert ($null -eq $zip.GetEntry($names[1])) 'oversized prior log omitted'
 Assert (@($manifest.omitted | Where-Object name -eq $names[1]).Count -eq 1) 'omission recorded in manifest'
 $logs=@($zip.Entries | Where-Object FullName -match '^dishonored_vr.*\.log$' | ForEach-Object FullName)
 Assert (($logs -join ',') -eq ($names[0]+','+$names[2])) 'game logs appear newest to oldest'
 foreach($name in @($names[0],$names[2])) {
  Assert ((Hash (Read-Entry $zip $name)) -eq (Hash ([IO.File]::ReadAllBytes((Join-Path $game $name))))) "$name survives byte for byte"
 }
} finally {$zip.Dispose()}
# Real production cap, oversized current log. Keep build banner AND last event.
$current=Join-Path $game 'dishonored_vr.log';Random-File $current 32000000
$stream=[IO.File]::Open($current,'Open','Write')
try {$header=$ascii.GetBytes('BUILD-BANNER-1.0.0');$tail=$ascii.GetBytes('LAST-EVENT-END');$stream.Write($header,0,$header.Length);[void]$stream.Seek(-$tail.Length,'End');$stream.Write($tail,0,$tail.Length)}finally{$stream.Dispose()}
$zip=Collect 'large-current'
try {
 $manifest=Manifest $zip;$entry=@($manifest.files | Where-Object name -eq 'dishonored_vr.log')[0]
 Assert ($entry.truncated -and $entry.sourceBytes -eq 32000000) 'large current log explicitly marked as excerpt'
 $bytes=Read-Entry $zip 'dishonored_vr.log'
 Assert ($ascii.GetString($bytes,0,18).StartsWith('BUILD-BANNER-1.0.0')) 'startup/build banner retained'
 Assert ($ascii.GetString($bytes,$bytes.Length-14,14).EndsWith('LAST-EVENT-END')) 'latest event retained'
 Assert ($ascii.GetString($bytes,65536,150).Contains('middle omitted')) 'excerpt has a visible omission marker'
 Assert ((Hash $bytes) -eq $entry.sha256) 'manifest hash matches the actual excerpt'
} finally {$zip.Dispose()}
# A file locked against readers must not abort collection of other evidence.
$locked=[IO.File]::Open($current,'Open','ReadWrite',[IO.FileShare]::None)
try {$zip=Collect 'unreadable'}finally{$locked.Dispose()}
try {
 $manifest=Manifest $zip
 Assert (@($manifest.omitted | Where-Object name -eq 'dishonored_vr.log').Count -eq 1) 'unreadable log recorded and skipped'
 Assert ($null -ne $zip.GetEntry('dishonored_vr.ini')) 'other evidence remains collectable'
} finally {$zip.Dispose()}
# No INI, plus a developer path on a drive that is not mounted, are optional evidence.
[IO.File]::Delete((Join-Path $game 'dishonored_vr.ini'))
$missing=Join-Path $root 'no-ini'
& "$PSScriptRoot\collect-support.ps1" -GameDir $game -DataDir 'Z:\dvr-nonexistent-test' -OutDir $missing -MaxZipBytes 1048576 -NoOpen | Out-Host
Assert (@(Get-ChildItem $missing -Filter '*.zip').Count -eq 1) 'missing INI and unavailable data drive do not prevent collection'
"support-budget-tests: $checks checks passed ($root)"
