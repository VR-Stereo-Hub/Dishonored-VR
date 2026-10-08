# The embedded data-file install policy (core/util/embedded_assets.cpp) on the host. Never launches the game.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\embedded-assets-test'
New-Item -ItemType Directory -Force -Path $out | Out-Null
[IO.File]::WriteAllBytes((Join-Path $out 'payload.bin'), [byte[]](1..200 | ForEach-Object { $_ % 251 }))
"#include <windows.h>`r`n9101 RCDATA ""payload.bin""`r`n" | Set-Content -Encoding ascii (Join-Path $out 'payload.rc')
. (Join-Path $PSScriptRoot 'lib\msvc.ps1')
$vc = Get-DvrMsvcRoot
$sdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$lib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$bin = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'x86\rc.exe') } | Sort-Object Name -Descending | Select-Object -First 1).FullName
$oldInclude=$env:INCLUDE; $oldLib=$env:LIB
Push-Location $out
try {
    $env:INCLUDE="$vc\include;$sdk\ucrt;$sdk\shared;$sdk\um"
    $env:LIB="$vc\lib\x86;$lib\ucrt\x86;$lib\um\x86"
    & (Join-Path $bin 'x86\rc.exe') /nologo /fo payload.res payload.rc
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed' }
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++17 /EHsc /W3 /DDVR_EMBEDDED_ASSET_MASK=1 "/I$PSScriptRoot\embedded-assets-stub" "/I$repo\src" /Fe:embedded_assets_test.exe (Join-Path $PSScriptRoot 'embedded-assets-tests.cpp') payload.res
    if ($LASTEXITCODE -ne 0) { throw 'Embedded assets test compilation failed' }
    & .\embedded_assets_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Embedded assets tests failed' }
} finally { Pop-Location; $env:INCLUDE=$oldInclude; $env:LIB=$oldLib }
