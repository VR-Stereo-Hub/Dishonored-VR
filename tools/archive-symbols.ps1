# Preserve binary/PDB pair for player dump analysis. Excluded from player ZIP.
param(
 [string]$BinDir="$PSScriptRoot\..\build\src\RelWithDebInfo",
 [string]$OutDir="$PSScriptRoot\..\build\symbol-archive"
)
$ErrorActionPreference='Stop'
$dll=Join-Path $BinDir 'd3d9.dll'
$pdb=Join-Path $BinDir 'd3d9.pdb'
if(-not(Test-Path $dll) -or -not(Test-Path $pdb)){throw 'Matching build DLL and PDB are required.'}
$hash=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLower()
$target=Join-Path $OutDir $hash
New-Item -ItemType Directory -Path $target -Force | Out-Null
foreach($source in @($dll,$pdb)){
 $dest=Join-Path $target (Split-Path -Leaf $source)
 if(Test-Path -LiteralPath $dest){
  if((Get-FileHash -LiteralPath $dest).Hash -ne (Get-FileHash -LiteralPath $source).Hash){throw 'Existing symbol archive differs; refusing overwrite.'}
 }else{Copy-Item -LiteralPath $source -Destination $dest}
}
@{ dllSHA256=$hash; pdbSHA256=(Get-FileHash -LiteralPath $pdb).Hash; archivedUtc=[DateTime]::UtcNow.ToString('o'); fileVersion=(Get-Item -LiteralPath $dll).VersionInfo.FileVersion } | ConvertTo-Json | Set-Content (Join-Path $target 'manifest.json')
Write-Output "Symbols archived: $target"
