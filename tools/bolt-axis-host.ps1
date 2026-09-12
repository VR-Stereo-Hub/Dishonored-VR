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
$transport=$pipeline.IndexOf('if (!g_config.modelRay && g_config.followHandTrim')
$model=$pipeline.IndexOf('if (g_config.modelRay && g_ray.ok)')
$publication=$pipeline.IndexOf('g_fireFrame = frame;')
$visual=$pipeline.IndexOf('auto out = visual(g_ray,')
if($transport -lt 0 -or $model -lt $transport -or $publication -lt $model -or $visual -lt $publication){throw 'Visual/fire publication regression'}
exit $eyeExit
