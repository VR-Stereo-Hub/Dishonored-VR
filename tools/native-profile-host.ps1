# Compile and exercise the production native draw profiler; never launch the game.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\native-profile-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 "/I$PSScriptRoot\native-profile-stubs" "/I$repo\src" /Fe:native_profile_test.exe (Join-Path $PSScriptRoot 'native-profile-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Profile test compilation failed.' }
    & .\native_profile_test.exe
    $eyeExit = $LASTEXITCODE
    if ($eyeExit -ne 0) { throw 'Profile test failed.' }
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
