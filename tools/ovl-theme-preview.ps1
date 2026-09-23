# VR-197: render a sample F10 panel with the real theme (src/core/ui/ovl_ui.cpp) offscreen,
# so the look can be judged without a headset. Writes build\ovl-theme-preview\*.png.
#   .\tools\ovl-theme-preview.ps1            (the Basic view)
#   .\tools\ovl-theme-preview.ps1 -Advanced  (the Advanced view)
param([switch]$Full, [switch]$Layout, [switch]$Zoom, [switch]$Advanced, [switch]$DebugTier, [int]$Height = 0, [ValidateRange(640,2048)][int]$Size = 1254, [switch]$Tooltip, [switch]$Bottom, [float]$TextScale = 0)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\ovl-theme-preview'
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$vc = Get-DvrMsvcRoot
$sdkInc = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$imgui = Join-Path $repo 'third_party\imgui'
$oldInc = $env:INCLUDE; $oldLib = $env:LIB
Push-Location $out
try {
    $env:INCLUDE = "$vc\include;$sdkInc\ucrt;$sdkInc\shared;$sdkInc\um;$sdkInc\winrt"
    $env:LIB = "$vc\lib\x86;$sdkLib\ucrt\x86;$sdkLib\um\x86"
    $srcs = @(
        (Join-Path $PSScriptRoot 'ovl-theme-preview.cpp'),
        (Join-Path $repo 'src\core\ui\ovl_ui.cpp'),
        (Join-Path $repo 'src\core\util\log.cpp'),
        "$imgui\imgui.cpp", "$imgui\imgui_draw.cpp", "$imgui\imgui_widgets.cpp", "$imgui\imgui_tables.cpp",
        "$imgui\backends\imgui_impl_dx11.cpp")
    $rc = Join-Path ($sdkInc -replace '\\Include\\', '\bin\') 'x64\rc.exe'
    & $rc /nologo /i $repo /fo ovl_assets.res (Join-Path $repo 'src\core\ui\ovl_assets.rc')
    if ($LASTEXITCODE -ne 0) { throw 'art resources failed' }
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /O1 /W3 /DNOMINMAX /I (Join-Path $repo 'src') /I $imgui /I "$imgui\backends" `
        $srcs ovl_assets.res /Fe:ovl_theme_preview.exe /link ole32.lib windowscodecs.lib d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib | Tee-Object -FilePath compile.log
    if ($LASTEXITCODE -ne 0) { throw 'ovl-theme-preview compilation failed' }
    $mode = if ($Full) { 'layout-full' } elseif ($Layout) { if ($Zoom) { 'layout-zoom' } else { 'layout' } } elseif ($DebugTier) { 'debug' } elseif ($Advanced) { 'advanced' } else { 'basic' }
    $tipMode = if ($Tooltip) { "tip" } elseif ($Bottom) { "bottom" } else { "none" }
    if ($TextScale -le 0) { $TextScale=1.54 * $Size / 1254 }
    if ($Height -le 0) { $Height=$Size }
    & .\ovl_theme_preview.exe $mode $Size $tipMode $TextScale $Height
    if ($LASTEXITCODE -ne 0) { throw 'ovl-theme-preview failed to render' }
    Add-Type -AssemblyName System.Drawing
    $png = Join-Path $out "ovl-theme-preview-$mode-$Size-$tipMode.png"
    $bmp = [System.Drawing.Bitmap]::new((Join-Path $out 'ovl-theme-preview.bmp'))
    $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    "Wrote $png"
} finally {
    Pop-Location
    $env:INCLUDE = $oldInc; $env:LIB = $oldLib
}
