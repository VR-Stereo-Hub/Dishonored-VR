# Compile and run the rounded wrist mesh builder and wheel crop bounds on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\rounded-wrist-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/hands/mesh_split.cpp'))
$pieces=@()
foreach($name in @('MsCapVertex','MsRoundedEnd')) {
    $body=[regex]::Match($text,('(?ms)^static (?:uint32_t|bool) '+$name+'\(.*?^\}'))
    if(-not $body.Success){throw "Production function extraction failed: $name"}
    $pieces+=$body.Value
}
[IO.File]::WriteAllText((Join-Path $out 'rounded_wrist_production.inc'),($pieces -join "`n"))
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$root = Get-DvrMsvcRoot
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" |
        Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x86;$libv\ucrt\x86;$libv\um\x86"
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:rounded-wrist-tests.exe (Join-Path $PSScriptRoot "rounded-wrist-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "rounded-wrist compilation failed." }
    .\rounded-wrist-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "rounded-wrist tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
