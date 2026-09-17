# Compile and run the controller emulation policy on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\controller-emulation-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$bridge=[IO.File]::ReadAllText((Join-Path $repo 'src/core/input/pad_bridge.cpp'))
$face=[regex]::Match($bridge,'(?ms)^        if \(in\.a\).*?(?=^        if \(SprintBit)')
if(-not $face.Success){throw 'Production face-button mapping extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'controller_face.inc'),$face.Value)
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:controller-emulation-tests.exe (Join-Path $PSScriptRoot "controller-emulation-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "controller-emulation compilation failed." }
    .\controller-emulation-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "controller-emulation tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
