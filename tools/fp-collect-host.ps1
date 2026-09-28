# Offline boundary test for the production candidate scan; never launches game.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo 'build/fp-collect-tests'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/hands/fp_mesh.cpp'))
$body=[regex]::Match($text,'(?ms)        const bool rangeReady=RangeReadable.*?if \(!IsLiveObject\(c\)\) continue;')
if(-not $body.Success){throw 'Production pointer scan extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'fp_scan.inc'),$body.Value)
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /O2 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:fp-collect-tests.exe (Join-Path $PSScriptRoot "fp-collect-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "fp-collect compilation failed." }
    .\fp-collect-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "fp-collect tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
