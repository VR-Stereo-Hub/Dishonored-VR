# Compile production load/pawn and name-lookup functions against fake engine data.
param([switch]$LegacyPawn, [switch]$LegacyNames)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\load-startup-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$eyeBody = ""
foreach ($unit in @(@('src/game/dishonored/crouch.cpp', @('PawnForCollision','ReadPawnCollisionHeight','PawnCollisionHeight','PawnCollisionTick','PawnSetCollisionHeight')), @('src/game/dishonored/ue3/uobject.cpp', @('FindNameIdx')))) {
    $unitText = [IO.File]::ReadAllText((Join-Path $repo $unit[0]))
    foreach ($fn in $unit[1]) {
        $match = [regex]::Match($unitText, ('(?ms)^static (?:uint8_t\*|float|uint32_t|void|bool) ' + $fn + '\(.*?^\}'))
        if (-not $match.Success) { throw "Production function not found: $fn" }
        $eyeBody += $match.Value + "`n"
    }
}
[IO.File]::WriteAllText((Join-Path $eyeOut 'load_startup_body.inc'), $eyeBody, [Text.UTF8Encoding]::new($false))
$eyeVc = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*' -Directory |
    Sort-Object Name -Descending | Select-Object -First 1).FullName
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /I (Join-Path $repo 'src') /Fe:load_startup_test.exe (Join-Path $PSScriptRoot 'load-startup-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Load-startup test compilation failed.' }
    & .\load_startup_test.exe ([int][bool]$LegacyPawn) ([int][bool]$LegacyNames)
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
