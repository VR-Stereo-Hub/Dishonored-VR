# Compile and run head movement policy on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\head-movement-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$root = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*" -Directory |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" |
        Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x86;$libv\ucrt\x86;$libv\um\x86"
& python (Join-Path $PSScriptRoot "head-movement-extract.py") (Join-Path $repo "src/game/dishonored/arm_follow.cpp") (Join-Path $out "handler.inc")
if ($LASTEXITCODE -ne 0) { throw "Handler extraction failed" }
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /Fe:head-movement-tests.exe (Join-Path $PSScriptRoot "head-movement-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "Head movement policy compilation failed." }
    .\head-movement-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "Head movement policy tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
