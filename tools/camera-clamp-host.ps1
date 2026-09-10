# Compile the production camera writer and ceiling-clamp regression cases.
param([string]$Source = "", [switch]$LegacyClamp)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Source) { $Source = Join-Path $repo 'src\game\dishonored\camera.cpp' }
$eyeOut = Join-Path $repo 'build\camera-clamp-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$eyeText = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Source))
$eyeBody = [regex]::Match($eyeText, '(?ms)^struct Writer \{.*?^\};').Value
foreach ($camFn in @('current_base', 'write_offset', 'clamp_written_z', 'restore')) {
    $camMatch = [regex]::Match($eyeText, ('(?ms)^(?:bool|void) ' + $camFn + '\(.*?^\}'))
    if (-not $camMatch.Success) { throw "Production function not found: $camFn" }
    if ($LegacyClamp -and $camFn -eq 'clamp_written_z') {
        # The original FovLeverApply wrote Z directly without updating Writer.
        $eyeBody += "`nstatic bool clamp_written_z(uint8_t* cam, uint32_t off, float z, Writer*) { if (!cam) return false; float* p = (float*)(cam + off); if (p[2] > z) p[2] = z; return false; }`n"
    } else { $eyeBody += "`n" + $camMatch.Value + "`n" }
}
if (-not $eyeBody.Contains('struct Writer')) { throw 'Production Writer type not found.' }
[IO.File]::WriteAllText((Join-Path $eyeOut 'camera_clamp_body.inc'), $eyeBody, [Text.UTF8Encoding]::new($false))
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /Fe:camera_clamp_test.exe (Join-Path $PSScriptRoot 'camera-clamp-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Camera-clamp test compilation failed.' }
    & .\camera_clamp_test.exe
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
