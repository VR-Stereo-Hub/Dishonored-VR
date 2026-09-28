# Native D3D9Ex test only. Never launches the game or touches its files.
$ErrorActionPreference = 'Stop'
if (Get-Process -Name Dishonored -ErrorAction SilentlyContinue) { throw 'Do not run GPU smoke tests during a game playtest.' }
$repo = Split-Path -Parent $PSScriptRoot
$desktopOut = Join-Path $repo 'build\shared-capture-native-test'
New-Item -ItemType Directory -Force -Path $desktopOut | Out-Null
$desktopVc = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*' -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopSdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopOldInclude=$env:INCLUDE
$desktopOldLib=$env:LIB
Push-Location $desktopOut
try {
    $env:INCLUDE="$desktopVc\include;$desktopSdk\ucrt;$desktopSdk\shared;$desktopSdk\um"
    $env:LIB="$desktopVc\lib\x86;$desktopLib\ucrt\x86;$desktopLib\um\x86"
    & "$desktopVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++17 /EHsc /W3 "/I$repo\src" /Fe:shared_capture_native_test.exe (Join-Path $PSScriptRoot 'shared-capture-native-tests.cpp') /link user32.lib d3d11.lib dxgi.lib
    if ($LASTEXITCODE -ne 0) { throw 'Native shared capture test compile failed.' }
    & .\shared_capture_native_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native shared capture test failed.' }
} finally {
    Pop-Location
    $env:INCLUDE=$desktopOldInclude
    $env:LIB=$desktopOldLib
}
exit 0
