# Production half-slot/Present service with a mocked XR cycle; never launches the game.
param([switch]$OldControl)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\msw-slot-test'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $repo 'src\core\vr\openxr_runtime.cpp'))
$bodies = foreach ($name in @('msw_half_slot','cycle_enter','cycle_leave')) {
    $body = [regex]::Match($source, "(?ms)^(?:bool|void) $name\(.*?^\}")
    if (-not $body.Success) { throw "Production $name body not found" }
    $body.Value
}
[IO.File]::WriteAllText((Join-Path $out 'msw_slot_body.inc'), ($bodies -join "`n"), [Text.UTF8Encoding]::new($false))
. (Join-Path $PSScriptRoot 'lib\msvc.ps1')
$vc = Get-DvrMsvcRoot
$sdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$lib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$oldInclude=$env:INCLUDE; $oldLib=$env:LIB
Push-Location $out
try {
    $env:INCLUDE="$vc\include;$sdk\ucrt;$sdk\shared;$sdk\um"
    $env:LIB="$vc\lib\x86;$lib\ucrt\x86;$lib\um\x86"
    $extra=@(); if($OldControl){$extra+= '/DOLD_CONTROL'}
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++17 /EHsc /W4 /I. @extra /Fe:msw_slot_test.exe (Join-Path $PSScriptRoot 'msw-slot-tests.cpp')
    if($LASTEXITCODE -ne 0){throw 'MSW slot test compilation failed'}
    & .\msw_slot_test.exe
    $result=$LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE=$oldInclude; $env:LIB=$oldLib
}
exit $result
