# The hand swap ([Hands] SwapHands, docs/dishonored/HAND_SWAP.md): offline checks. Never launches
# the game.
#
# 1. frame_test: the hand frame suite the proxy also runs at every start, including the
#    swap_mirror_* cases (hand_frame_test.h) that pin the mirrored placement itself.
# 2. hand-swap-tests.cpp: the consumers of the mirrored hand (the aim ray, the view-aligned trim
#    step, the weapons, full-arm IK's plane, the effects), each against a render-side reference
#    and each with a negative control.
# Both compile the PRODUCTION headers, so the thing under test is what ships.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$hsOut = Join-Path $repo 'build\hand-swap-test'
New-Item -ItemType Directory -Force -Path $hsOut | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$hsVc = Get-DvrMsvcRoot
$hsSdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$hsLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$hsOldInclude = $env:INCLUDE
$hsOldLib = $env:LIB
Push-Location $hsOut
try {
    $env:INCLUDE = "$hsVc\include;$hsSdk\ucrt;$hsSdk\shared;$hsSdk\um"
    $env:LIB = "$hsVc\lib\x86;$hsLib\ucrt\x86;$hsLib\um\x86"
    $cl = "$hsVc\bin\Hostx64\x86\cl.exe"
    & $cl /nologo /std:c++20 /EHsc /W3 /I. "/I$repo\src" /Fe:frame_test.exe `
        (Join-Path $repo 'src\tools\frame_test\main.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'frame_test compilation failed.' }
    & .\frame_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'frame_test FAILED.' }
    & $cl /nologo /std:c++20 /EHsc /W3 /I. "/I$repo\src" /Fe:hand_swap_test.exe `
        (Join-Path $PSScriptRoot 'hand-swap-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Hand-swap test compilation failed.' }
    & .\hand_swap_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Hand-swap checks FAILED.' }
}
finally {
    Pop-Location
    $env:INCLUDE = $hsOldInclude
    $env:LIB = $hsOldLib
}
