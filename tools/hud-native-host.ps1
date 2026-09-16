# Compile and run the native HUD draw scope on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\hud-native-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/hud_class.cpp'))
$body=[regex]::Match($text,'(?ms)^struct NativeIconScope \{.*?^\};')
if(-not $body.Success){throw 'Native scope extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_native_scope.inc'),$body.Value)
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:hud-native-tests.exe (Join-Path $PSScriptRoot "hud-native-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "hud-native compilation failed." }
    .\hud-native-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "hud-native tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
