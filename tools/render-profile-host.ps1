# Compile the production desktop pin policy and exercise pixel sequences.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\render-profile-test'
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 "/I$PSScriptRoot\render-profile-stubs" "/I$repo\src" /Fe:render_profile_test.exe (Join-Path $PSScriptRoot 'render-profile-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Profile test compilation failed.' }
    & .\render_profile_test.exe
    $eyeExit = $LASTEXITCODE
    if ($eyeExit -ne 0) { throw 'Profile test failed.' }
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
