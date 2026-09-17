# Compile and run the native HUD draw scope on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\objective-marker-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/objective_markers.cpp'))
$stub=[regex]::Match($text,'(?ms)^__declspec\(noinline\) void __fastcall TaskParentStub.*?^\}')
$fp=[regex]::Match($text,'(?ms)^bool TaskParentFingerprint.*?^\}')
if(-not $stub.Success -or -not $fp.Success){throw 'Production wrapper extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'objective_stub.inc'),$stub.Value)
[IO.File]::WriteAllText((Join-Path $out 'objective_fingerprint.inc'),$fp.Value)
$runeStub=[regex]::Match($text,'(?ms)^__declspec\(noinline\) void __fastcall RuneParentStub.*?^\}')
$runeFp=[regex]::Match($text,'(?ms)^bool RuneParentFingerprint.*?^\}')
if(-not $runeStub.Success -or -not $runeFp.Success){throw 'Rune wrapper extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'rune_stub.inc'),$runeStub.Value)
[IO.File]::WriteAllText((Join-Path $out 'rune_fingerprint.inc'),$runeFp.Value)
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:objective-marker-tests.exe (Join-Path $PSScriptRoot "objective-marker-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "objective-marker compilation failed." }
    .\objective-marker-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "objective-marker tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
