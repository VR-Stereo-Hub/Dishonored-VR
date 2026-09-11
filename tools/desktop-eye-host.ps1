# Compile the production desktop pin policy and exercise pixel sequences.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\desktop-eye-test'
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /Fe:desktop_eye_test.exe (Join-Path $PSScriptRoot 'desktop-eye-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Desktop-eye test compilation failed.' }
    & .\desktop_eye_test.exe
    $eyeExit = $LASTEXITCODE
    if ($eyeExit -ne 0) { throw 'Desktop policy regression failed.' }
    & .\desktop_eye_test.exe --legacy-must-pin
    if ($LASTEXITCODE -ne 1) { throw 'Negative control did not reject the legacy delayed-tag pin.' }
    Write-Host 'PASS: negative control rejected the legacy policy as expected.'
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 "/I$PSScriptRoot\desktop-eye-stubs" "/I$repo\src" /Fe:desktop_eye_copy_test.exe (Join-Path $PSScriptRoot 'desktop-eye-copy-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Desktop copy test compilation failed.' }
    & .\desktop_eye_copy_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Desktop copy test failed.' }
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
