# Compile and run the production animation-origin-route math and lifecycle on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\animation-origin-route-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$source=[IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/hands/mesh_split.cpp'))
$start=$source.IndexOf('static dvr::anim::OriginTranslation g_animOrigin;')
$end=$source.IndexOf('// Place one hand through an already-acquired draw context.',$start)
if($start -lt 0 -or $end -le $start){throw 'Production origin adapter not found'}
[IO.File]::WriteAllText((Join-Path $out 'animation_origin_route.inc'),$source.Substring($start,$end-$start))
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$root = Get-DvrMsvcRoot
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" |
        Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x86;$libv\ucrt\x86;$libv\um\x86"
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:animation-origin-route-tests.exe (Join-Path $PSScriptRoot "animation-origin-route-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "animation-origin-route compilation failed." }
    .\animation-origin-route-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "animation-origin-route tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
