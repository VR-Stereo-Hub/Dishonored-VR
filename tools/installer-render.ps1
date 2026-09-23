# VR-198: draw every screen of DishonoredVR-Setup.exe headless and save PNGs, so the
# look and the layout can be judged without a game, a headset or a click.
# Writes build\installer-preview\<state>.png (and <state>@1.5.png for a 144-dpi
# layout). The states are the named fakes in src/tools/installer/model/fake_states.cpp.
#   .\tools\installer-render.ps1                 (every state, 1.0 and 1.5 scale)
#   .\tools\installer-render.ps1 -State done     (one state)
#   .\tools\installer-render.ps1 -Debug          (the Debug build of the exe)
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [string]$State = 'all',
    [switch]$Debug,
    [double[]]$Scales = @(1.0, 1.5)
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$config = if ($Debug) { 'Debug' } else { 'RelWithDebInfo' }
$exe = Join-Path $repo "build\src\$config\DishonoredVR-Setup.exe"
if (-not (Test-Path $exe)) { throw "missing $exe - run tools\build.ps1 first" }
$out = Join-Path $repo 'build\installer-preview'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Add-Type -AssemblyName System.Drawing
$wrote = 0
foreach ($scale in $Scales) {
    $bmpDir = Join-Path $out ("bmp-{0}" -f $scale)
    New-Item -ItemType Directory -Force -Path $bmpDir | Out-Null
    $target = if ($State -eq 'all') { $bmpDir } else { Join-Path $bmpDir "$State.bmp" }
    # The exe is a windows-subsystem app: it attaches to this console for its output.
    $p = Start-Process -FilePath $exe -ArgumentList @('--render', $State, "`"$target`"", '--scale', $scale) -Wait -PassThru -NoNewWindow
    if ($p.ExitCode -ne 0) { throw "render failed for state '$State' at scale $scale (exit $($p.ExitCode))" }
    foreach ($bmp in Get-ChildItem $bmpDir -Filter *.bmp) {
        $suffix = if ($scale -eq 1.0) { '' } else { "@$scale" }
        $png = Join-Path $out ($bmp.BaseName + $suffix + '.png')
        $img = [System.Drawing.Bitmap]::new($bmp.FullName)
        $img.Save($png, [System.Drawing.Imaging.ImageFormat]::Png); $img.Dispose()
        Remove-Item $bmp.FullName
        $wrote++
    }
    Remove-Item $bmpDir -Recurse -Force
}
"wrote $wrote PNGs under $out"
