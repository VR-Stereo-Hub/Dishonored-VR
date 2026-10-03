param([Parameter(Mandatory=$true)][string]$DownloadFile)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $repo ('build/reshade-install-tests-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
[IO.File]::WriteAllText((Join-Path $fixture 'Dishonored.exe'),'fixture only; never executed')
[IO.File]::WriteAllText((Join-Path $fixture 'd3d9.dll'),'existing mod')
[IO.File]::WriteAllText((Join-Path $fixture 'ReShade.ini'),'existing preset and settings')
$helper=Join-Path $PSScriptRoot 'install-reshade.ps1'
$ps=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$source=(Resolve-Path -LiteralPath $DownloadFile).Path
& $ps -NoProfile -ExecutionPolicy Bypass -File $helper -GameDir $fixture -DownloadFile $source
if($LASTEXITCODE -ne 0){throw 'valid download failed'}
$runtime=Join-Path $fixture 'ReShade32.dll'
$hash=(Get-FileHash -LiteralPath $runtime).Hash
if($hash -ne 'DA430E0A9C6EECEFA0D1B27D05E16C426FB5D04E808B194D914EAAC4B31BC0F8'){throw 'extracted runtime mismatch'}
# Exercise the launcher environment failure without depending on this host's module path.
$probe=Join-Path $fixture 'without-hash-cmdlet.ps1'
$probeText="function Get-FileHash { throw 'Hash cmdlet is unavailable in the launcher environment' }`r`n"
$probeText += "& '"+$helper.Replace("'","''")+"' -GameDir '"+$fixture.Replace("'","''")+"' -DownloadFile '"+$source.Replace("'","''")+"'"
[IO.File]::WriteAllText($probe,$probeText)
# Remove only our extracted fixture runtime so this case does not add a backup.
Remove-Item -LiteralPath $runtime
& $ps -NoProfile -ExecutionPolicy Bypass -File $probe
if($LASTEXITCODE -ne 0 -or (Get-FileHash -LiteralPath $runtime).Hash -ne $hash){throw 'launcher hash independence failed'}
[IO.File]::WriteAllText($runtime,'older runtime fixture')
& $ps -NoProfile -ExecutionPolicy Bypass -File $helper -GameDir $fixture -DownloadFile $source
if($LASTEXITCODE -ne 0){throw 'replacement failed'}
$backup=@(Get-ChildItem -LiteralPath $fixture -Filter '*.dvr-backup')
if($backup.Count -ne 1 -or [IO.File]::ReadAllText($backup[0].FullName) -ne 'older runtime fixture'){throw 'previous runtime was not backed up'}
$bad=Join-Path $fixture 'bad.exe';[IO.File]::WriteAllText($bad,'invalid download; never executed')
& $ps -NoProfile -ExecutionPolicy Bypass -File $helper -GameDir $fixture -DownloadFile $bad
if($LASTEXITCODE -ne 1 -or (Get-FileHash -LiteralPath $runtime).Hash -ne $hash){throw 'hash failure changed runtime'}
if([IO.File]::ReadAllText((Join-Path $fixture 'd3d9.dll')) -ne 'existing mod' -or [IO.File]::ReadAllText((Join-Path $fixture 'ReShade.ini')) -ne 'existing preset and settings'){throw 'existing mod/preset changed'}
'PASS ReShade extraction, PE32 check, backup, hash rejection and existing mod/preset preservation'
