# Native D3D9Ex test only. Never launches the game or touches its files.
$ErrorActionPreference = 'Stop'
if (Get-Process -Name Dishonored -ErrorAction SilentlyContinue) { throw 'Do not run GPU smoke tests during a game playtest.' }
$repo = Split-Path -Parent $PSScriptRoot
$desktopOut = Join-Path $repo 'build\reshade-manual-test'
New-Item -ItemType Directory -Force -Path $desktopOut | Out-Null
$runtime = 'C:\Program Files (x86)\Steam\steamapps\common\Dishonored\Binaries\Win32\ReShade32.dll'
if (!(Test-Path -LiteralPath $runtime)) { throw 'Install official ReShade before running the optional host test.' }
Copy-Item -LiteralPath $runtime -Destination (Join-Path $desktopOut 'ReShade32.dll')
"[ReShade]`r`nManualRuntime=1`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'dishonored_vr.ini')
"[GENERAL]`r`nPresetPath=.\test-preset.ini`r`nEffectSearchPaths=.\`r`nTextureSearchPaths=.\`r`nPerformanceMode=1`r`n[OVERLAY]`r`nTutorialProgress=4`r`nShowFPS=0`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'ReShade.ini')
"Techniques=Invert@Test.fx`r`nTechniqueSorting=Invert@Test.fx`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'test-preset.ini')
@'
texture BackBufferTex : COLOR;
sampler BackBuffer { Texture = BackBufferTex; };
void Fullscreen(uint id : SV_VertexID, out float4 pos : SV_Position, out float2 uv : TEXCOORD) {
    uv = float2(id == 2 ? 2.0 : 0.0, id == 1 ? 2.0 : 0.0);
    pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 InvertPS(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target { return float4(1 - tex2D(BackBuffer, uv).rgb, 1); }
technique Invert { pass { VertexShader = Fullscreen; PixelShader = InvertPS; } }
'@ | Set-Content -LiteralPath (Join-Path $desktopOut 'Test.fx')
$desktopVc = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*' -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopSdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopOldInclude=$env:INCLUDE
$desktopOldLib=$env:LIB
Push-Location $desktopOut
try {
    $env:INCLUDE="$desktopVc\include;$desktopSdk\ucrt;$desktopSdk\shared;$desktopSdk\um"
    $env:LIB="$desktopVc\lib\x86;$desktopLib\ucrt\x86;$desktopLib\um\x86"
    & "$desktopVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++17 /EHsc /W3 /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS "/I$repo\src" /Fe:reshade_manual_test.exe (Join-Path $PSScriptRoot 'reshade-manual-tests.cpp') /link user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Native desktop test compile failed.' }
    & .\reshade_manual_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native desktop test failed.' }
    "Techniques=`r`nTechniqueSorting=Invert@Test.fx`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'test-preset.ini')
    & .\reshade_manual_test.exe --disabled
    if ($LASTEXITCODE -ne 0) { throw 'Disabled-effects native test failed.' }
} finally {
    Pop-Location
    $env:INCLUDE=$desktopOldInclude
    $env:LIB=$desktopOldLib
}
exit 0

