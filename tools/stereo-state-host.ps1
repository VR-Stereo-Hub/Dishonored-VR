# Compile and run stereo state policy on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\stereo-state-tests"
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
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /Fe:stereo-state-tests.exe (Join-Path $PSScriptRoot "stereo-state-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "Stereo state policy compilation failed." }
    .\stereo-state-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "Stereo state policy tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
