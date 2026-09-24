# VR-214: exercise the downloaded EXE's real replacement entry point, no GUI/game.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$versionText=Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
if($versionText -notmatch 'project\(DishonoredVR VERSION ([0-9.]+)') {throw 'Cannot read version'}
$exe=Join-Path $repo "build\src\RelWithDebInfo\DishonoredVR-Launcher-v$($Matches[1]).exe"
$sha=(Get-FileHash $exe).Hash.ToLower()
$scratch=Join-Path $env:TEMP ('dvr-launcher-handoff-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$target=Join-Path $scratch 'DishonoredVR-Launcher.exe'
$old=[byte[]](77,90,1,2,3,4,5)
[IO.File]::WriteAllBytes($target,$old)
$oldSha=(Get-FileHash $target).Hash
function Assert($condition,$message) {if(!$condition){throw $message}; "  ok   $message"}
$parent=Start-Process -FilePath "$env:SystemRoot\System32\ping.exe" -ArgumentList @('-n','4','127.0.0.1') -WindowStyle Hidden -PassThru
$argsList=@('--complete-update',"`"$target`"",'--sha256',$sha,'--parent-pid',$parent.Id,'--no-restart')
$child=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru
Start-Sleep -Milliseconds 300
Assert ((Get-FileHash $target).Hash -eq $oldSha) 'target unchanged while parent is alive'
$child.WaitForExit()
Assert ($child.ExitCode -eq 0) 'real update helper exits successfully'
Assert ((Get-FileHash $target).Hash.ToLower() -eq $sha) 'updated launcher matches candidate byte for byte'
$backups=@(Get-ChildItem -LiteralPath $scratch -Filter '*.previous-*')
Assert ($backups.Count -eq 1 -and (Get-FileHash $backups[0].FullName).Hash -eq $oldSha) 'previous launcher is preserved exactly'
$badArgs=@('--complete-update',"`"$target`"",'--sha256',('0'*64),'--no-restart','--result',"`"$(Join-Path $scratch 'failure.txt')`"")
$p=Start-Process -FilePath $exe -ArgumentList $badArgs -WindowStyle Hidden -PassThru -Wait
Assert ($p.ExitCode -eq 2 -and (Get-FileHash $target).Hash.ToLower() -eq $sha) 'bad checksum refuses without touching the launcher'
$locked=[IO.File]::Open($target,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    $p=Start-Process -FilePath $exe -ArgumentList @('--complete-update',"`"$target`"",'--sha256',$sha,'--no-restart') -WindowStyle Hidden -PassThru -Wait
    Assert ($p.ExitCode -eq 2) 'locked launcher refuses replacement'
} finally {$locked.Dispose()}
Assert ((Get-FileHash $target).Hash.ToLower() -eq $sha) 'locked launcher stays byte identical'
"launcher-handoff-smoke: all passed ($scratch)"
