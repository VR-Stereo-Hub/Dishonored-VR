# VR-57 commit 2: offline checks for follow_trim_ray. Never launches the game.
#
# Compiles the PRODUCTION header, so the thing under test is what ships. The
# reference inside the test is built independently from palm_target plus an
# explicit change of basis, so the M cancellation the production path relies on
# has to be right for the two to agree. Toolchain located the same way the other
# host harnesses do it.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$ftOut = Join-Path $repo 'build\follow-trim-test'
New-Item -ItemType Directory -Force -Path $ftOut | Out-Null
$ftVc = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*' -Directory |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$ftSdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$ftLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$ftOldInclude = $env:INCLUDE
$ftOldLib = $env:LIB
Push-Location $ftOut
try {
    $env:INCLUDE = "$ftVc\include;$ftSdk\ucrt;$ftSdk\shared;$ftSdk\um"
    $env:LIB = "$ftVc\lib\x86;$ftLib\ucrt\x86;$ftLib\um\x86"
    & "$ftVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. "/I$repo\src" `
        /Fe:follow_trim_test.exe (Join-Path $PSScriptRoot 'follow-trim-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Follow-trim test compilation failed.' }
    & .\follow_trim_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Follow-trim checks FAILED.' }
}
finally {
    Pop-Location
    $env:INCLUDE = $ftOldInclude
    $env:LIB = $ftOldLib
}
