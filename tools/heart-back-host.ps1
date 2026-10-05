param([string]$Reference = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\heart-back-test'
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot 'lib\msvc.ps1')
$vc = Get-DvrMsvcRoot
$sdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$lib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$oldInclude=$env:INCLUDE; $oldLib=$env:LIB
Push-Location $out
try {
    $env:INCLUDE="$vc\include;$sdk\ucrt;$sdk\shared;$sdk\um"
    $env:LIB="$vc\lib\x86;$lib\ucrt\x86;$lib\um\x86"
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++17 /EHsc /W4 "/I$repo\src" /Fe:heart_back_test.exe (Join-Path $PSScriptRoot 'heart-back-tests.cpp')
    if($LASTEXITCODE -ne 0){throw 'Heart test compilation failed'}
    if ($Reference) { & .\heart_back_test.exe $Reference } else { & .\heart_back_test.exe }
    if ($LASTEXITCODE -ne 0) { throw 'Heart tests failed' }
} finally { Pop-Location; $env:INCLUDE=$oldInclude; $env:LIB=$oldLib }
