# Compile production single-tag identity and slot management against fake objects.
param([switch]$Legacy)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\single-tag-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$eyeBody = ""
$captureText = [IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/capture.cpp'))
$body = [regex]::Match($captureText, '(?ms)^bool retire_last_right_grab\(.*?^\}')
if (-not $body.Success) { throw 'Capture guard function not found' }
$eyeBody = $body.Value
[IO.File]::WriteAllText((Join-Path $eyeOut 'single_tag_body.inc'), $eyeBody, [Text.UTF8Encoding]::new($false))
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /I (Join-Path $repo 'src') /Fe:single_tag_test.exe (Join-Path $PSScriptRoot 'single-tag-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Single-tag test compilation failed.' }
    & .\single_tag_test.exe ([int][bool]$Legacy)
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
