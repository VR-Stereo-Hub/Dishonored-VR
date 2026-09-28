# Test the production semantic branch that precedes heuristic marker routing.
param([switch]$NegativeControl)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo 'build\markers-routing-tests'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/hud_layout.cpp'))
$body=[regex]::Match($text,'(?ms)^    if\(!g_visualRiding && dvr::hudowner::active\(\)\) \{.*?^    \}')
if(-not $body.Success){throw 'Semantic route branch not found'}
[IO.File]::WriteAllText((Join-Path $out 'semantic_route.inc'),("int route(int* elementOut,float* nativePivot,bool* nativeMarker) { if(nativeMarker)*nativeMarker=false;`n"+$body.Value+"`nreturn -2;}`n"))
if($NegativeControl) {
 $file=Join-Path $out 'semantic_route.inc'
 $old=[IO.File]::ReadAllText($file).Replace('if(nativeMarker)*nativeMarker=marker;','')
 [IO.File]::WriteAllText($file,$old)
}
. (Join-Path $PSScriptRoot 'lib\msvc.ps1')
$vc=Get-DvrMsvcRoot
$sdk=(Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$lib=(Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$oldInclude=$env:INCLUDE;$oldLib=$env:LIB
Push-Location $out
try {
 $env:INCLUDE="$vc\include;$sdk\ucrt;$sdk\shared;$sdk\um"
 $env:LIB="$vc\lib\x86;$lib\ucrt\x86;$lib\um\x86"
 & "$vc\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /I (Join-Path $repo 'src') /Fe:markers-routing-tests.exe (Join-Path $PSScriptRoot 'markers-routing-tests.cpp')
 if($LASTEXITCODE -ne 0){throw 'Routing test compile failed'}
 .\markers-routing-tests.exe
 if($NegativeControl) {
  if($LASTEXITCODE -eq 0){throw 'Old-code negative control unexpectedly passed'}
  Write-Host 'Old-code negative control fails as expected: semantic marker flag is missing.'
 } elseif($LASTEXITCODE -ne 0){throw 'Routing test failed'}
} finally {Pop-Location;$env:INCLUDE=$oldInclude;$env:LIB=$oldLib}
