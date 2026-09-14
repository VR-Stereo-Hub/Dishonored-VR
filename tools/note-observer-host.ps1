# Compile production note-observer identity and slot management against fake objects.
param([switch]$LegacyObserver)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\note-observer-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$eyeBody = ""
$uiText = [IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/ue3/ui_state.cpp'))
if ($LegacyObserver) {
    $uiText = (git -C $repo show HEAD:src/game/dishonored/ue3/ui_state.cpp) -join "`n"
}
foreach ($fn in @('UiInstanceLive','UiResetInstances','UiAddInstance')) {
    $source = $uiText
    if ($LegacyObserver -and $fn -ne 'UiAddInstance') {
        $source = [IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/ue3/ui_state.cpp'))
    }
    $match = [regex]::Match($source, ('(?ms)^static (?:void|bool) ' + $fn + '\(.*?^\}'))
    if (-not $match.Success) { throw "Production function not found: $fn" }
    $eyeBody += $match.Value + "`n"
}
$viewText = [IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/commands.cpp'))
$view = [regex]::Match($viewText, '(?ms)^static bool DvrScriptViewLive\(.*?^\}')
if (-not $view.Success) { throw 'Production view-state function not found' }
$eyeBody += $view.Value + "`n"
$stateText = [IO.File]::ReadAllText((Join-Path $repo 'src/mod/state/62_game_dishonored_ue3_ui_state.inc'))
$struct = [regex]::Match($stateText, '(?ms)^struct UiInst \{.*?^\};')
$eyeBody = $struct.Value + "`nstatic UiInst g_uiInst[UI_INST_MAX];`n" + $eyeBody
[IO.File]::WriteAllText((Join-Path $eyeOut 'note_observer_body.inc'), $eyeBody, [Text.UTF8Encoding]::new($false))
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$eyeVc = Get-DvrMsvcRoot
$eyeSdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$eyeLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$eyeOldInclude = $env:INCLUDE
$eyeOldLib = $env:LIB
Push-Location $eyeOut
try {
    $env:INCLUDE = "$eyeVc\include;$eyeSdk\ucrt;$eyeSdk\shared;$eyeSdk\um"
    $env:LIB = "$eyeVc\lib\x86;$eyeLib\ucrt\x86;$eyeLib\um\x86"
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /I (Join-Path $repo 'src') /Fe:note_observer_test.exe (Join-Path $PSScriptRoot 'note-observer-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Load-startup test compilation failed.' }
    & .\note_observer_test.exe ([int][bool]$LegacyObserver)
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
