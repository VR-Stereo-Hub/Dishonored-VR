$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot ('..\build\support-fixture-'+[Guid]::NewGuid().ToString('N'))
$game=Join-Path $root 'game with spaces'
$data=Join-Path $root 'data with spaces'
New-Item -ItemType Directory -Path $game,(Join-Path $data 'dumps') -Force | Out-Null
("[Paths]"+[Environment]::NewLine+"DataDir=$data"+[Environment]::NewLine) | Set-Content (Join-Path $game 'dishonored_vr.ini')
'fixture log' | Set-Content (Join-Path $game 'dishonored_vr.log')
'never include game content' | Set-Content (Join-Path $game 'secret.upk')
'fixture dump' | Set-Content (Join-Path $data 'dumps\fixture.dmp')
# VR-177: the watchdog's trace and its extract
@('[12:00:00.000] pace line','[12:00:01.000] WATCHDOG presentsStopped stuck 4000 ms tid=1') | Set-Content (Join-Path $data 'pacetrace.log')
$stream=[IO.File]::Open((Join-Path $game 'dishonored_vr.log'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
try { & "$PSScriptRoot\collect-support.ps1" -GameDir $game -OutDir (Join-Path $root 'output') -NoOpen } finally { $stream.Dispose() }
$zip=Get-ChildItem (Join-Path $root 'output') -Filter '*.zip' | Select-Object -First 1
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($zip.FullName)
try {
 $names=@($archive.Entries | ForEach-Object {$_.FullName})
 foreach($required in @('dishonored_vr.log','dishonored_vr.ini','manifest.json','READ-ME.txt','pacetrace.log','pacetrace-watchdog.txt')) { if($names -notcontains $required){throw "Missing $required"} }
 if($names -contains 'fixture.dmp' -or $names -contains 'secret.upk'){throw 'Unexpected private/game content'}
 $reader=[IO.StreamReader]::new($archive.GetEntry('manifest.json').Open())
 try {$manifest=$reader.ReadToEnd() | ConvertFrom-Json}finally{$reader.Dispose()}
 foreach($file in $manifest.files) {
  if($file.truncated -or $file.sourceBytes -ne $file.copiedBytes){throw "Small source unexpectedly shortened: $($file.name)"}
 }
 $entry=$archive.GetEntry('dishonored_vr.log');$reader=[IO.StreamReader]::new($entry.Open())
 try {if($reader.ReadToEnd().Trim() -ne 'fixture log'){throw 'Log content changed'}}finally{$reader.Dispose()}
 if($manifest.errors.Count -or $manifest.dataDir -ne $data -or $manifest.dumpInventory.Count -ne 1){throw 'Manifest/directory resolution failed'}
} finally {$archive.Dispose()}
& "$PSScriptRoot\collect-support.ps1" -GameDir $game -OutDir (Join-Path $root 'with-dump') -IncludeLatestDump -NoOpen
$zip=Get-ChildItem (Join-Path $root 'with-dump') -Filter '*.zip' | Select-Object -First 1
$archive=[IO.Compression.ZipFile]::OpenRead($zip.FullName)
try {if(-not $archive.GetEntry('fixture.dmp')){throw 'Explicit dump inclusion failed'}}finally{$archive.Dispose()}
'PASS: locked log, spaces, INI DataDir, missing optional files, whitelist, dump opt-in, ZIP manifest, watchdog trace + extract'
