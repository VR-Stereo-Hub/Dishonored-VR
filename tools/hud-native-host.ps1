# Compile and run the native HUD draw scope on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\hud-native-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/hud_class.cpp'))
$body=[regex]::Match($text,'(?ms)^struct NativeIconScope \{.*?^\};')
if(-not $body.Success){throw 'Native scope extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_native_scope.inc'),$body.Value)
$layout=[IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/hud_layout.cpp'))
$policy=[regex]::Matches($layout,'(?m)^(bool native_gameplay_reference|bool native_objective_upright|float native_objective_scale)\(.*$')
if($policy.Count -ne 3){throw 'Native reference policy extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_native_policy.inc'),(($policy | ForEach-Object {$_.Value}) -join "`n"))
$tutorial=[regex]::Match($layout,'(?ms)^inline bool semantic_tutorial\(.*?^\}')
$eligible=[regex]::Match($layout,'(?m)^inline bool crop_eligible\(.*$')
if(-not $tutorial.Success -or -not $eligible.Success){throw 'Semantic panel extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_semantic_panel.inc'),$tutorial.Value+"`n"+$eligible.Value)
$capture=[IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/hud_capture.cpp'))
$health=[regex]::Match($capture,'(?ms)^void note_native_reference.*?(?=^bool redirect_failed)')
if(-not $health.Success){throw 'Native reference health extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_reference_health.inc'),$health.Value)
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
