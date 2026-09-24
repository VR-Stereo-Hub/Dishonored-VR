# VR-215: run the exact GUI collector helper through its noninteractive entry point.
param([string]$OldLauncher)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$versionText=Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
if($versionText -notmatch 'project\(DishonoredVR VERSION ([0-9.]+)'){throw 'Cannot read version'}
$exe=Join-Path $repo "build\src\RelWithDebInfo\DishonoredVR-Launcher-v$($Matches[1]).exe"
$root=Join-Path ([IO.Path]::GetTempPath()) ('dvr-support-native-'+[Guid]::NewGuid().ToString('N'))
$game=Join-Path $root ('game '+[char]0x00e9+' space')
$data=Join-Path $root 'data';$fresh=Join-Path $root 'fresh temp'
New-Item -ItemType Directory -Path $game,$data,$fresh | Out-Null
[IO.File]::WriteAllText((Join-Path $game 'dishonored_vr.ini'),"[Paths]`r`nDataDir=$data`r`n",[Text.Encoding]::ASCII)
for($i=0;$i -lt 10;$i++) {
 $name=if($i -eq 0){'dishonored_vr.log'}elseif($i -eq 1){'dishonored_vr.prev.log'}else{"dishonored_vr.prev$i.log"}
 $path=Join-Path $game $name;[IO.File]::WriteAllText($path,"SESSION-$i`r`n",[Text.Encoding]::ASCII)
 [IO.File]::SetLastWriteTimeUtc($path,[DateTime]::UtcNow.AddMinutes(-$i))
}
$savedTemp=$env:TEMP;$savedTmp=$env:TMP;$savedData=$env:DVR_DATA_DIR
function Invoke-Collector($launcher,$name) {
 $output=Join-Path $root $name;$result=Join-Path $root "$name.txt"
 $p=Start-Process -FilePath $launcher -ArgumentList @('--collect-logs','--game-dir',"`"$game`"",'--support-out',"`"$output`"",'--result',"`"$result`"") -WindowStyle Hidden -Wait -PassThru
 return [pscustomobject]@{code=$p.ExitCode;text=[IO.File]::ReadAllText($result);output=$output}
}
function Assert($value,$message){if(!$value){throw $message};Write-Host "  ok   $message"}
Add-Type -AssemblyName System.IO.Compression.FileSystem
try {
 $env:TEMP=$fresh;$env:TMP=$fresh;$env:DVR_DATA_DIR=$data
 if($OldLauncher) {
  $old=Invoke-Collector $OldLauncher 'old'
  Assert ($old.code -ne 0 -and $old.text -match '\(3\)') 'released 1.0.0 reproduces error 3 on a fresh temp profile'
  Assert (-not (Test-Path -LiteralPath (Join-Path $fresh 'DishonoredVR-Launcher'))) 'missing parent is the failing prerequisite'
 }
 $good=Invoke-Collector $exe ('new '+[char]0x00e9)
 Assert ($good.code -eq 0 -and $good.text.StartsWith('Support bundle: ')) 'new launcher collects through real helper on the same fresh profile'
 $zip=Get-ChildItem $good.output -Filter '*.zip' | Select-Object -First 1
 Assert ($good.text.Contains($zip.FullName)) 'non-ASCII output path survives native stdout and UTF-8 notice'
 Assert ($zip.Length -lt 25000000) 'native collector ZIP is under 25 MB'
 $archive=[IO.Compression.ZipFile]::OpenRead($zip.FullName)
 try {
  $logs=@($archive.Entries | Where-Object FullName -match '^dishonored_vr.*\.log$')
  Assert ($logs.Count -eq 10) 'all ten game sessions included'
  for($i=0;$i -lt 10;$i++) {
   $reader=[IO.StreamReader]::new($logs[$i].Open())
   try {Assert ($reader.ReadToEnd().Trim() -eq "SESSION-$i") "session $i has exact content in newest-first order"}finally{$reader.Dispose()}
  }
 }finally{$archive.Dispose()}
 # TEMP is a file, so no child directory can be created. The app-data fallback must work.
 $blocked=Join-Path $root 'blocked-temp';[IO.File]::WriteAllText($blocked,'not a directory')
 $env:TEMP=$blocked;$env:TMP=$blocked
 $fallback=Invoke-Collector $exe 'fallback'
 Assert ($fallback.code -eq 0 -and $fallback.text.StartsWith('Support bundle: ')) 'unusable TEMP falls back successfully'
 "support-launcher-tests: all passed ($root)"
}finally{$env:TEMP=$savedTemp;$env:TMP=$savedTmp;$env:DVR_DATA_DIR=$savedData}
