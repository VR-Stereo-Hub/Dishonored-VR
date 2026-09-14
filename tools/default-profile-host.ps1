# Execute the production default ini writer in a standalone 32-bit host.
param()
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\default-profile-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$config = [IO.File]::ReadAllText((Join-Path $repo 'src/core/config/config.cpp'))
$body = [regex]::Match($config, '(?ms)^static void WriteDefaultIni\(.*?^\}')
if (-not $body.Success) { throw 'Default ini writer not found' }
[IO.File]::WriteAllText((Join-Path $eyeOut 'default_profile_body.inc'), $body.Value, [Text.UTF8Encoding]::new($false))
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /I (Join-Path $repo 'src') /I (Join-Path $repo 'build\generated') /Fe:default_profile_test.exe (Join-Path $PSScriptRoot 'default-profile-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Default-profile test compilation failed.' }
    & .\default_profile_test.exe
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
if ($eyeExit -ne 0) { exit $eyeExit }
$actualHash = (Get-FileHash (Join-Path $eyeOut 'actual.ini') -Algorithm SHA256).Hash
foreach ($profile in @('release/dishonored_vr.ini','tests/golden/dishonored_vr.ini')) {
    if ((Get-FileHash (Join-Path $repo $profile) -Algorithm SHA256).Hash -ne $actualHash) {
        throw "Production writer differs byte-for-byte from $profile"
    }
}
'Production writer, packaged profile and golden INI are byte-identical.'
exit 0
