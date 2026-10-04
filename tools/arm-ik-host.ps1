param([string]$Reference = '', [string]$SweepOutput = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\arm-ik-test'
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
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++17 /EHsc /W4 "/I$repo\src" /Fe:arm_ik_test.exe (Join-Path $PSScriptRoot 'arm-ik-tests.cpp')
    if($LASTEXITCODE -ne 0){throw 'Arm IK test compilation failed'}
    if ($Reference) { & .\arm_ik_test.exe $Reference } else { & .\arm_ik_test.exe }
    $result=$LASTEXITCODE
    if ($result -eq 0 -and $SweepOutput) {
        if (-not $Reference) { throw 'Sweep requires a local reference file' }
        & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++17 /EHsc /W4 "/I$repo\src" /Fe:arm_ik_sweep.exe (Join-Path $PSScriptRoot 'arm-ik-sweep.cpp')
        if($LASTEXITCODE -ne 0){throw 'Arm IK sweep compilation failed'}
        & .\arm_ik_sweep.exe $Reference $SweepOutput
        $result=$LASTEXITCODE
    }
} finally {
    Pop-Location
    $env:INCLUDE=$oldInclude; $env:LIB=$oldLib
}
exit $result
