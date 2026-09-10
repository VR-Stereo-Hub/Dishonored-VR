# Compile the real eye-decision body with independent render/script inputs.
param([string]$Source = "")
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Source) { $Source = Join-Path $repo 'src\game\dishonored\hands\mesh_split.cpp' }
$eyeOut = Join-Path $repo 'build\palette-eye-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$eyeText = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Source))
$eyeBody = [regex]::Match($eyeText, '(?ms)^static void MpEyeForPresent\(const MpDrawCtx\* c\)\r?\n\{.*?^\}')
if (-not $eyeBody.Success) { throw 'Production eye decision was not found.' }
[IO.File]::WriteAllText((Join-Path $eyeOut 'palette_eye_body.inc'), $eyeBody.Value, [Text.UTF8Encoding]::new($false))
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /Fe:palette_eye_test.exe (Join-Path $PSScriptRoot 'palette-eye-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Eye-decision test compilation failed.' }
    & .\palette_eye_test.exe
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
