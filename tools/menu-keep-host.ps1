# Compile the production VR-93 menu lifecycle (hands/menu_keep.h) with synthetic
# state sequences and a fake object table that frees and reuses addresses.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\menu-keep-test'
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$vc = Get-DvrMsvcRoot
$sdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$lib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
Push-Location $out
try {
    $env:INCLUDE = "$vc\include;$sdk\ucrt;$sdk\shared;$sdk\um"
    $env:LIB = "$vc\lib\x86;$lib\ucrt\x86;$lib\um\x86"
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I (Join-Path $repo 'src') `
        /Fe:menu_keep_test.exe (Join-Path $PSScriptRoot 'menu-keep-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'menu-keep test compilation failed.' }
    & .\menu_keep_test.exe
    $code = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $oldInclude
    $env:LIB = $oldLib
}
exit $code
