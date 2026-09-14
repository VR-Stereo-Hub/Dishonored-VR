# Test standing/crouched positional math without launching the game.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\mono-ui-test'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vc = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*' -Directory |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
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
        /Fe:mono_ui_test.exe (Join-Path $PSScriptRoot 'mono-ui-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'mono UI test compilation failed.' }
    & .\mono_ui_test.exe
    $code = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $oldInclude
    $env:LIB = $oldLib
}
exit $code
