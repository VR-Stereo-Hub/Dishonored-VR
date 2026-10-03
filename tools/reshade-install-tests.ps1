param([Parameter(Mandatory=$true)][string]$DownloadFile,
      # A folder holding standard.zip, sweetfx.zip and prod80.zip - the pinned packages,
      # byte-identical to the downloads (their hashes are checked). Keeps the test offline.
      [Parameter(Mandatory=$true)][string]$PackageDir)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$helper=Join-Path $PSScriptRoot 'install-reshade.ps1'
$ps=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$source=(Resolve-Path -LiteralPath $DownloadFile).Path
$packages=(Resolve-Path -LiteralPath $PackageDir).Path
function New-Fixture {
    $f=Join-Path $repo ('build/reshade-install-tests-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $f | Out-Null
    [IO.File]::WriteAllText((Join-Path $f 'Dishonored.exe'),'fixture only; never executed')
    [IO.File]::WriteAllText((Join-Path $f 'd3d9.dll'),'existing mod')
    return $f
}
function Install([string]$dir,[string]$download=$source,[string]$pkg=$packages) {
    & $ps -NoProfile -ExecutionPolicy Bypass -File $helper -GameDir $dir -DownloadFile $download -PackageDir $pkg | Out-Null
    return $LASTEXITCODE
}

# ---- an existing setup: runtime extraction, backups, hash rejection, nothing of the player's changed
$fixture=New-Fixture
[IO.File]::WriteAllText((Join-Path $fixture 'ReShade.ini'),'existing preset and settings')
if((Install $fixture) -ne 0){throw 'valid download failed'}
$runtime=Join-Path $fixture 'ReShade32.dll'
$hash=(Get-FileHash -LiteralPath $runtime).Hash
if($hash -ne 'DA430E0A9C6EECEFA0D1B27D05E16C426FB5D04E808B194D914EAAC4B31BC0F8'){throw 'extracted runtime mismatch'}
# Exercise the launcher environment failure without depending on this host's module path.
$probe=Join-Path $fixture 'without-hash-cmdlet.ps1'
$probeText="function Get-FileHash { throw 'Hash cmdlet is unavailable in the launcher environment' }`r`n"
$probeText += "& '"+$helper.Replace("'","''")+"' -GameDir '"+$fixture.Replace("'","''")+"' -DownloadFile '"+$source.Replace("'","''")+"' -PackageDir '"+$packages.Replace("'","''")+"'"
[IO.File]::WriteAllText($probe,$probeText)
# Remove only our extracted fixture runtime so this case does not add a backup.
Remove-Item -LiteralPath $runtime
& $ps -NoProfile -ExecutionPolicy Bypass -File $probe | Out-Null
if($LASTEXITCODE -ne 0 -or (Get-FileHash -LiteralPath $runtime).Hash -ne $hash){throw 'launcher hash independence failed'}
# An identical runtime is not replaced, so a repeated install adds no backup.
if((Install $fixture) -ne 0){throw 'repeat install failed'}
if(@(Get-ChildItem -LiteralPath $fixture -Filter '*.dvr-backup').Count -ne 0){throw 'identical runtime was backed up'}
[IO.File]::WriteAllText($runtime,'older runtime fixture')
if((Install $fixture) -ne 0){throw 'replacement failed'}
$backup=@(Get-ChildItem -LiteralPath $fixture -Filter '*.dvr-backup')
if($backup.Count -ne 1 -or [IO.File]::ReadAllText($backup[0].FullName) -ne 'older runtime fixture'){throw 'previous runtime was not backed up'}
$bad=Join-Path $fixture 'bad.exe';[IO.File]::WriteAllText($bad,'invalid download; never executed')
if((Install $fixture $bad) -ne 1 -or (Get-FileHash -LiteralPath $runtime).Hash -ne $hash){throw 'hash failure changed runtime'}
if([IO.File]::ReadAllText((Join-Path $fixture 'd3d9.dll')) -ne 'existing mod' -or [IO.File]::ReadAllText((Join-Path $fixture 'ReShade.ini')) -ne 'existing preset and settings'){throw 'existing mod/preset changed'}

# ---- a fresh install (the 1.0.3 fault): ReShade.ini, the three packages and the custom folders appear
$fresh=New-Fixture
if((Install $fresh) -ne 0){throw 'fresh install failed'}
$ini=Join-Path $fresh 'ReShade.ini'
if(-not (Test-Path -LiteralPath $ini)){throw 'fresh install wrote no ReShade.ini - ReShade will refuse to load'}
$text=[IO.File]::ReadAllText($ini)
foreach($want in 'EffectSearchPaths=.\dvr-reshade-shaders\standard\Shaders','custom\Shaders\**','KeyEffects=145,0,0,0','PerformanceMode=1'){
    if(-not $text.Contains($want)){throw "ReShade.ini lacks $want"}
}
if($text.Contains('PresetPath')){throw 'PresetPath written without a preset present'}
foreach($rel in 'standard\Shaders\ReShade.fxh','sweetfx\Shaders\SweetFX','prod80\Shaders','custom\Shaders','custom\Textures'){
    if(-not (Test-Path -LiteralPath (Join-Path $fresh "dvr-reshade-shaders\$rel"))){throw "missing dvr-reshade-shaders\$rel"}
}
# Every search path in the ini names a folder that exists (the ** suffix searches below it).
foreach($line in ($text -split "`r`n" | Where-Object { $_ -match '^(Effect|Texture)SearchPaths=' })){
    foreach($p in ($line.Split('=',2)[1].Split(','))){
        $dir=Join-Path $fresh ($p.TrimEnd('*').TrimEnd('\').TrimStart('.').TrimStart('\'))
        if(-not (Test-Path -LiteralPath $dir)){throw "search path does not exist: $p"}
    }
}
if(@(Get-ChildItem -LiteralPath (Join-Path $fresh 'dvr-reshade-shaders') -Filter '*.tmp').Count){throw 'staging folder left behind'}
# A repeat keeps everything the first run made and the player may have changed.
[IO.File]::WriteAllText($ini,'player edited')
[IO.File]::WriteAllText((Join-Path $fresh 'dvr-reshade-shaders\custom\Shaders\Mine.fx'),'player shader')
if((Install $fresh) -ne 0){throw 'repeat fresh install failed'}
if([IO.File]::ReadAllText($ini) -ne 'player edited' -or -not (Test-Path -LiteralPath (Join-Path $fresh 'dvr-reshade-shaders\custom\Shaders\Mine.fx'))){throw 'repeat install changed player files'}

# ---- a Carinth preset already beside the exe is selected in a new ReShade.ini
$carinth=New-Fixture
[IO.File]::WriteAllText((Join-Path $carinth 'DishonoredCarinthPresetv3.ini'),'Techniques=')
if((Install $carinth) -ne 0){throw 'carinth install failed'}
if(-not [IO.File]::ReadAllText((Join-Path $carinth 'ReShade.ini')).Contains('PresetPath=.\DishonoredCarinthPresetv3.ini')){throw 'Carinth preset not selected'}

# ---- a package whose bytes do not match its pin is refused; the rest still installs
$tampered=Join-Path $repo ('build/reshade-install-tests-pkg-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tampered | Out-Null
Copy-Item (Join-Path $packages 'standard.zip'),(Join-Path $packages 'prod80.zip') $tampered
[IO.File]::WriteAllText((Join-Path $tampered 'sweetfx.zip'),'not the pinned package')
$partial=New-Fixture
if((Install $partial $source $tampered) -ne 1){throw 'tampered package was not reported as a failure'}
if(Test-Path -LiteralPath (Join-Path $partial 'dvr-reshade-shaders\sweetfx')){throw 'tampered package was extracted'}
if(-not (Test-Path -LiteralPath (Join-Path $partial 'dvr-reshade-shaders\prod80\Shaders'))){throw 'good packages were not installed beside the failed one'}
if(-not (Test-Path -LiteralPath (Join-Path $partial 'ReShade.ini'))){throw 'ReShade.ini missing after a partial package failure'}

'PASS ReShade extraction, PE32 check, backup (none for an identical runtime), hash rejection, existing mod/preset preservation, fresh-install ReShade.ini + packages + custom folders, Carinth selection, tampered package refusal'
