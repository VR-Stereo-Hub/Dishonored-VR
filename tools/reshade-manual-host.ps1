# Native D3D9Ex test only. Never launches the game or touches its files.
$ErrorActionPreference = 'Stop'
if (Get-Process -Name Dishonored -ErrorAction SilentlyContinue) { throw 'Do not run GPU smoke tests during a game playtest.' }
$repo = Split-Path -Parent $PSScriptRoot
$desktopOut = Join-Path $repo 'build\reshade-manual-test'
New-Item -ItemType Directory -Force -Path $desktopOut | Out-Null
$overlaySource = [IO.File]::ReadAllText((Join-Path $repo 'src/core/ui/overlay.cpp'))
$tweak = [regex]::Match($overlaySource, '(?ms)^static void OvlUpdateSliderTweak\(\).*?^\}')
if (!$tweak.Success) { throw 'Production F10 slider tweak function not found.' }
[IO.File]::WriteAllText((Join-Path $desktopOut 'overlay_slider_tweak.inc'), $tweak.Value)
$runtime = 'C:\Program Files (x86)\Steam\steamapps\common\Dishonored\Binaries\Win32\ReShade32.dll'
if (!(Test-Path -LiteralPath $runtime)) { throw 'Install official ReShade before running the optional host test.' }
Copy-Item -LiteralPath $runtime -Destination (Join-Path $desktopOut 'ReShade32.dll')
"[ReShade]`r`nEnabled=1`r`nManualRuntime=1`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'dishonored_vr.ini')
"[GENERAL]`r`nPresetPath=.\test-preset.ini`r`nEffectSearchPaths=.\`r`nTextureSearchPaths=.\`r`nPerformanceMode=1`r`nSkipLoadingDisabledEffects=1`r`n[OVERLAY]`r`nTutorialProgress=4`r`nShowFPS=0`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'ReShade.ini')
"Techniques=Invert@Test.fx`r`nTechniqueSorting=Invert@Test.fx,Dormant@Dormant.fx,Unrelated@Unrelated.fx`r`n[Dormant.fx]`r`nStrength=1.0`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'test-preset.ini')
@'
uniform float Strength < ui_type="slider"; ui_min=0.0; ui_max=1.0; ui_label="Intensity"; > = 1.0;
texture BackBufferTex : COLOR;
sampler BackBuffer { Texture = BackBufferTex; };
void Fullscreen(uint id : SV_VertexID, out float4 pos : SV_Position, out float2 uv : TEXCOORD) {
    uv = float2(id == 2 ? 2.0 : 0.0, id == 1 ? 2.0 : 0.0);
    pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 InvertPS(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target { return float4(lerp(tex2D(BackBuffer, uv).rgb, 1 - tex2D(BackBuffer, uv).rgb, Strength), 1); }
technique Invert { pass { VertexShader = Fullscreen; PixelShader = InvertPS; } }
'@ | Set-Content -LiteralPath (Join-Path $desktopOut 'Test.fx')
# A separate disabled file reproduces startup skipping; another technique in Test.fx would not.
(Get-Content -Raw -LiteralPath (Join-Path $desktopOut 'Test.fx')).Replace('technique Invert', 'technique Dormant') | Set-Content -LiteralPath (Join-Path $desktopOut 'Dormant.fx')
(Get-Content -Raw -LiteralPath (Join-Path $desktopOut 'Test.fx')).Replace('technique Invert', 'technique Unrelated') | Set-Content -LiteralPath (Join-Path $desktopOut 'Unrelated.fx')
$desktopVc = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*' -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopSdk = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$desktopOldInclude=$env:INCLUDE
$desktopOldLib=$env:LIB
Push-Location $desktopOut
try {
    $env:INCLUDE="$desktopVc\include;$desktopSdk\ucrt;$desktopSdk\shared;$desktopSdk\um"
    $env:LIB="$desktopVc\lib\x86;$desktopLib\ucrt\x86;$desktopLib\um\x86"
    & "$desktopVc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /DIMGUI_ENABLE_TEST_ENGINE /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I. "/I$repo\src" "/I$repo\third_party\imgui" /Fe:reshade_manual_test.exe (Join-Path $PSScriptRoot 'reshade-manual-tests.cpp') (Join-Path $repo "src/core/ui/reshade_panel.cpp") (Join-Path $repo "third_party/imgui/imgui.cpp") (Join-Path $repo "third_party/imgui/imgui_draw.cpp") (Join-Path $repo "third_party/imgui/imgui_tables.cpp") (Join-Path $repo "third_party/imgui/imgui_widgets.cpp") /link user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Native desktop test compile failed.' }
    & .\reshade_manual_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native desktop test failed.' }
    # A second PROCESS reads the preset saved by F10, including its unchecked effect.
    & .\reshade_manual_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Persisted unchecked-effect restart test failed.' }
    "Techniques=`r`nTechniqueSorting=Invert@Test.fx,Dormant@Dormant.fx,Unrelated@Unrelated.fx`r`n[Dormant.fx]`r`nStrength=1.0`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'test-preset.ini')
    & .\reshade_manual_test.exe --disabled
    if ($LASTEXITCODE -ne 0) { throw 'Disabled-effects native test failed.' }
    "[ReShade]`r`nManualRuntime=1`r`n" | Set-Content -LiteralPath (Join-Path $desktopOut 'dishonored_vr.ini')
    & .\reshade_manual_test.exe --default-off
    if ($LASTEXITCODE -ne 0) { throw 'Default-off native test failed.' }

} finally {
    Pop-Location
    $env:INCLUDE=$desktopOldInclude
    $env:LIB=$desktopOldLib
}
exit 0

