# Compile the real diagnostic flight recorder with synthetic transport and runtime outcomes.

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\flicker-diagnostic-test'
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
    $diagArgs = @('/DDVR_FLICKER_DIAGNOSTICS=1')
    & "$vc\bin\Hostx64\x86\cl.exe" @diagArgs /nologo /std:c++20 /EHsc /W3 /I (Join-Path $repo 'src') `
        /Fe:flicker_diagnostic_test.exe (Join-Path $PSScriptRoot 'flicker-diagnostic-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'reentry-pair test compilation failed.' }
    & .\flicker_diagnostic_test.exe
    $code = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $oldInclude
    $env:LIB = $oldLib
}
exit $code
