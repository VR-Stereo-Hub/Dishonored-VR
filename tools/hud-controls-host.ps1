# Compile and run the HUD alpha ownership and reading input (VR-127) on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\hud-controls-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$layout=[IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/hud_layout.cpp'))
$selector=[regex]::Match($layout,'(?ms)^AlphaCfg alpha_for_sink\(.*?^\}')
$capture=[regex]::Match([IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/hud_class.cpp')),'(?m)^inline bool alpha_force_wanted.*$')
$menu=[regex]::Match([IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/game_state.cpp')),'(?ms)^static SHORT MenuStep\(.*?^\}')
if(-not $selector.Success -or -not $capture.Success -or -not $menu.Success){throw 'Production source extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_alpha_selector.inc'),$selector.Value)
[IO.File]::WriteAllText((Join-Path $out 'hud_alpha_capture.inc'),$capture.Value)
[IO.File]::WriteAllText((Join-Path $out 'hud_menu_step.inc'),$menu.Value)
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:hud-controls-tests.exe (Join-Path $PSScriptRoot "hud-controls-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "hud-controls compilation failed." }
    .\hud-controls-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "hud-controls tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
