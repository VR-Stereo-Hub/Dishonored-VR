# Compile and run production viewport resize guards on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\viewport-resize-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /Fe:viewport-resize-tests.exe (Join-Path $PSScriptRoot "viewport-resize-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "Viewport resize compilation failed." }
    .\viewport-resize-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "Viewport resize tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
