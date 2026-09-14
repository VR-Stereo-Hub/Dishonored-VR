# Exercise the production crawl-edge writer and its retained identity guard.
param([switch]$Legacy)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$eyeOut = Join-Path $repo 'build\crawl-strength-test'
New-Item -ItemType Directory -Force -Path $eyeOut | Out-Null
$sourceText = [IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/hands/skelcontrol.cpp'))
$identity = [regex]::Match($sourceText, '(?ms)^static inline bool SkcAlive\(.*?^\}')
$writer = [regex]::Match($sourceText, '(?ms)^static void SkcSetCrawlStrength\(.*?^\}')
if (-not $identity.Success -or -not $writer.Success) { throw 'Production functions not found' }
$legacyWriter = 'static void LegacySetCrawlStrength(float s) { if(!g_graftOffStr)return; for(int i=0;i<g_skcPlayerN && i<8;++i){auto o=g_skcPlayer[i]; if(!o || ((uintptr_t)o & 3) || !RangeReadable(o,g_graftOffStr+4))continue; *(float*)(o+g_graftOffStr)=s; if(g_graftOffSTgt && RangeReadable(o,g_graftOffSTgt+4))*(float*)(o+g_graftOffSTgt)=s; }}'
[IO.File]::WriteAllText((Join-Path $eyeOut 'crawl_strength_body.inc'), $identity.Value + "`r`n" + $writer.Value + "`r`n" + $legacyWriter, [Text.UTF8Encoding]::new($false))
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
    & "$eyeVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /I. /I (Join-Path $repo 'src') /Fe:crawl_strength_test.exe (Join-Path $PSScriptRoot 'crawl-strength-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Crawl-strength test compilation failed.' }
    if ($Legacy) { & .\crawl_strength_test.exe legacy } else { & .\crawl_strength_test.exe }
    $eyeExit = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $eyeOldInclude
    $env:LIB = $eyeOldLib
}
exit $eyeExit
