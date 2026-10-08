# Compile and run the grab animation's maths (src/game/dishonored/hands/grab_pose.h) on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\grab-pose-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$root = Get-DvrMsvcRoot
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" |
        Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x86;$libv\ucrt\x86;$libv\um\x86"
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I (Join-Path $repo "src") /Fe:grab-pose-tests.exe (Join-Path $PSScriptRoot "grab-pose-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "grab-pose compilation failed." }
    .\grab-pose-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "grab-pose tests failed." }
    # the same header behind a line protocol, for tools\blender\grab_verify.py
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /D_CRT_SECURE_NO_WARNINGS /I (Join-Path $repo "src") /Fe:grab-pose-cli.exe (Join-Path $PSScriptRoot "grab-pose-cli.cpp")
    if ($LASTEXITCODE -ne 0) { throw "grab-pose-cli compilation failed." }
    Write-Host "grab-pose: CLI at $out\grab-pose-cli.exe"
} finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
