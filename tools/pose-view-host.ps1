# Exercise the production position matcher and rotation tie resolver without the game.
param([switch]$OldControl)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\pose-view-test'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $repo 'src\core\vr\pose_record.cpp'))
$bodies = foreach ($name in @('find_view','resolve_view_tie')) {
    $body = [regex]::Match($source, "(?ms)^bool $name\(.*?^\}")
    if (-not $body.Success) { throw "Production $name body not found" }
    $body.Value
}
[IO.File]::WriteAllText((Join-Path $out 'pose_view_body.inc'), ($bodies -join "`n"), [Text.UTF8Encoding]::new($false))
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
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W4 /I. "/I$repo\src" @extra /Fe:pose_view_test.exe (Join-Path $PSScriptRoot 'pose-view-tests.cpp')
    if($LASTEXITCODE -ne 0){throw 'Pose view test compilation failed'}
    & .\pose_view_test.exe
    $result=$LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE=$oldInclude; $env:LIB=$oldLib
}
exit $result
