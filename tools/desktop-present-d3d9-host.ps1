# Native D3D9Ex test only. Never launches the game or touches its files.
$ErrorActionPreference = 'Stop'
if (Get-Process -Name Dishonored -ErrorAction SilentlyContinue) { throw 'Do not run GPU smoke tests during a game playtest.' }
$repo = Split-Path -Parent $PSScriptRoot
$desktopOut = Join-Path $repo 'build\desktop-present-d3d9-test'
New-Item -ItemType Directory -Force -Path $desktopOut | Out-Null
# Stub logging/status only; d3d9.h and the clock are the real SDK/production code.
foreach ($header in @('core/util/log.h','core/framework/status.h')) {
    $target = Join-Path $desktopOut "stubs/$header"
    New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "desktop-eye-stubs/$header") -Destination $target
}
$desktopVc = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*' -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopSdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopOldInclude=$env:INCLUDE
$desktopOldLib=$env:LIB
Push-Location $desktopOut
try {
    $env:INCLUDE="$desktopVc\include;$desktopSdk\ucrt;$desktopSdk\shared;$desktopSdk\um"
    $env:LIB="$desktopVc\lib\x86;$desktopLib\ucrt\x86;$desktopLib\um\x86"
    & "$desktopVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++17 /EHsc /W3 /Istubs "/I$repo\src" /Fe:desktop_present_d3d9_test.exe (Join-Path $PSScriptRoot 'desktop-present-d3d9-tests.cpp') (Join-Path $repo 'src/core/util/clock.cpp') /link user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Native desktop test compile failed.' }
    & .\desktop_present_d3d9_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native desktop test failed.' }
} finally {
    Pop-Location
    $env:INCLUDE=$desktopOldInclude
    $env:LIB=$desktopOldLib
}
exit 0
