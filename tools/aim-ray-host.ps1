# Compile the real ray math and extract the production XR point/layer builders.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\aim-ray-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$runtime = [IO.File]::ReadAllText((Join-Path $repo 'src/core/vr/openxr_runtime.cpp'))
$bodies = ''
foreach ($name in @('quat_facing','publish_laser_image','build_aim_point','note_aim_visual','build_aim_visual')) {
    $pattern = '(?ms)^(?:XrQuaternionf|bool|void|AimVisualResult) ' + $name + '\(.*?^\}'
    $body = [regex]::Match($runtime, $pattern)
    if (-not $body.Success) { throw "Production function missing: $name" }
    $bodies += $body.Value + "`r`n"
}
[IO.File]::WriteAllText((Join-Path $eyeOut 'aim_visual_bodies.inc'),$bodies,[Text.UTF8Encoding]::new($false))

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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. "/I$repo\src" "/I$repo\third_party\OpenXR-SDK\include" /Fe:aim_ray_test.exe (Join-Path $PSScriptRoot 'aim-ray-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Desktop-eye test compilation failed.' }
    & .\aim_ray_test.exe
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
