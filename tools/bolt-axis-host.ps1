# Offline crossbow geometry and actual x86 bridge ABI tests. Never launches the game.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\bolt-axis-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$eyeVc = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*' -Directory |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. "/I$repo\src" "/I$repo\third_party\OpenXR-SDK\include" /Fe:bolt_axis_test.exe (Join-Path $PSScriptRoot 'bolt-axis-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Fire-aim test compilation failed.' }
    & .\bolt_axis_test.exe
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
# Regression: the implementation that introduced FollowHandTrim published firing
# before transport. Keep this actual integration ordering in addition to math tests.
$pipeline=Get-Content (Join-Path $repo 'src/game/dishonored/aim_ray.cpp') -Raw
# The INVARIANT, not the old sequence. What must hold is that every block which can
# change the ray runs BEFORE the single fire publication, and that the visual is
# built AFTER it - so the guide and the shot cannot be given different rays, which
# is the defect this guard was added for. The precedence between the two ray sources
# was deliberately inverted afterwards (measured model axis first, controller ray as
# the fallback), so pinning their order would pin a decision rather than a contract.
$model=$pipeline.IndexOf('if (g_config.modelRay && g_ray.ok)')
$transport=$pipeline.IndexOf('if (!modelUsed && !g_config.modelRay && g_config.followHandTrim')
$publication=$pipeline.IndexOf('g_fireFrame = frame;')
$visual=$pipeline.IndexOf('auto out = visual(g_ray,')
if($model -lt 0){throw 'model ray block not found'}
if($transport -lt 0){throw 'controller-ray fallback block not found'}
if($publication -lt 0 -or $visual -lt 0){throw 'publication or visual not found'}
if($publication -lt $model){throw 'fire published before the model ray could change it'}
if($publication -lt $transport){throw 'fire published before the controller fallback could change it'}
if($visual -lt $publication){throw 'visual built before the fire publication - they can diverge'}
# The controller ray is the fallback for ModelRay being OFF. While ModelRay is ON and
# the axis has not latched yet, NO guide is shown - showing the controller ray there
# puts it in the wrong place and then moves it when the latch lands, which is the one
# behaviour the tester asked never to happen. Both halves are asserted.
if(-not $pipeline.Contains('waiting for the shared bolt axis to settle')){
    throw 'a pending model axis no longer suppresses the guide; it will jump when the axis latches'
}
if(-not $pipeline.Contains('!modelUsed && !g_config.modelRay && g_config.followHandTrim')){
    throw 'the controller fallback is no longer gated on ModelRay being off'
}
exit $eyeExit
