# Host tests for the DLSS object-motion pass (VR-39): tools\dlss-objmotion-tests.cpp against the production
# shaders in src\core\gfx\dlss_gpu.cpp. No NGX, no helper, never launches the game.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\dlss-objmotion-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$root = Get-DvrMsvcRoot
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" | Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" | Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE; $savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x86;$libv\ucrt\x86;$libv\um\x86"
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W3 /O2 /std:c++17 /I (Join-Path $repo "src") /Fe:dlss-objmotion-tests.exe `
        (Join-Path $PSScriptRoot "dlss-objmotion-tests.cpp") (Join-Path $repo "src\core\gfx\dlss_gpu.cpp") /link d3d11.lib dxgi.lib
    if ($LASTEXITCODE -ne 0) { throw "dlss objmotion test compilation failed." }
    .\dlss-objmotion-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "dlss objmotion tests failed." }
} finally {
    Pop-Location
    $env:INCLUDE = $savedInclude; $env:LIB = $savedLib
}
