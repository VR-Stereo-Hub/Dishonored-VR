# Exercise the production profile validator and guarded integer writer.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\game-opts-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$sourceText = [IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/game_opts.cpp'))
$functions = @('GoVerifyStride','GoWriteRaw','GoApplyWritesAndVerify') | ForEach-Object {
    $m = [regex]::Match($sourceText, "(?ms)^(?:static )?bool $_\(.*?^\}")
    if (-not $m.Success) { throw "Missing production function $_" }
    $m.Value
}
[IO.File]::WriteAllText((Join-Path $eyeOut 'game_opts_body.inc'), ($functions -join "`r`n"), [Text.UTF8Encoding]::new($false))
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$eyeVc = Get-DvrMsvcRoot
$eyeSdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$eyeLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
$eyeOldInclude = $env:INCLUDE
$eyeOldLib = $env:LIB
Push-Location $eyeOut
try {
    $env:INCLUDE = "$eyeVc\include;$eyeSdk\ucrt;$eyeSdk\shared;$eyeSdk\um"
    $env:LIB = "$eyeVc\lib\x86;$eyeLib\ucrt\x86;$eyeLib\um\x86"
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /I (Join-Path $repo 'src') /Fe:game_opts_test.exe (Join-Path $PSScriptRoot 'game-opts-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Game-options test compilation failed.' }
    & .\game_opts_test.exe
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
