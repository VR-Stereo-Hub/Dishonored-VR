# build-dxvk.ps1 - build the DXVK fork (dxvk\) into build\dxvk\dxvk_d3d9.dll.
#
# DXVK builds with meson. Upstream CI builds x86 on Windows with MSVC, and that
# is the path used here (one toolchain with the proxy, PDBs, no MSYS2):
#   - Visual Studio 2022 with the C++ workload (found via vswhere)
#   - meson + ninja on PATH        (python -m pip install meson ninja)
#   - glslangValidator.exe on PATH (Vulkan SDK, https://vulkan.lunarg.com)
#   - the DXVK submodules checked out (git submodule update --init --recursive)
# The original release binary was cross-compiled with MinGW-w64 from Linux
# (dxvk\build-win32.txt); -MinGW uses an MSYS2 install for that instead.
#
#   .\tools\build-dxvk.ps1 [-Debug] [-Clean] [-Install]
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [switch]$Debug,
    [switch]$Clean,
    [switch]$Install,
    [switch]$MinGW,
    [string]$Msys2 = "C:\msys64"
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$src = Join-Path $repo "dxvk"
$outDir = Join-Path $repo "build\dxvk"

if (-not (Test-Path (Join-Path $src "include\vulkan\include\vulkan\vulkan.h"))) {
    throw "dxvk submodules are not checked out - run: git submodule update --init --recursive"
}
$buildtype = if ($Debug) { "debug" } else { "release" }

if ($MinGW) {
    $bash = Join-Path $Msys2 "usr\bin\bash.exe"
    if (-not (Test-Path $bash)) { throw "MSYS2 not found at $Msys2 (pacman -S mingw-w64-i686-gcc mingw-w64-i686-meson mingw-w64-i686-glslang ninja)" }
    $bdir = "build-dxvk/mingw-x86"
    $cmd = "cd '$($repo -replace '\\','/')' && export PATH=/mingw32/bin:`$PATH && " +
           "( [ -d $bdir ] || meson setup $bdir dxvk --cross-file dxvk/build-win32.txt --buildtype $buildtype -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false ) && meson compile -C $bdir"
    & $bash -lc $cmd
    if ($LASTEXITCODE -ne 0) { throw "MinGW DXVK build failed" }
    $built = Join-Path $repo "build-dxvk\mingw-x86\src\d3d9\d3d9.dll"
} else {
    foreach ($tool in @("meson", "ninja", "glslangValidator")) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "$tool is not on PATH. meson/ninja: python -m pip install meson ninja; glslangValidator: install the Vulkan SDK."
        }
    }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
    if (-not $vs) { throw "Visual Studio 2022 not found" }
    $vsdevcmd = Join-Path $vs "Common7\Tools\VsDevCmd.bat"
    # Import the x86 developer environment into this process.
    $envDump = cmd /s /c "`"$vsdevcmd`" -arch=x86 -host_arch=x64 -no_logo && set"
    foreach ($line in $envDump) {
        if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
    }
    $bdir = Join-Path $repo "build-dxvk\msvc-x86"
    if ($Clean -and (Test-Path $bdir)) { Remove-Item $bdir -Recurse -Force }
    Push-Location $repo
    try {
        if (-not (Test-Path (Join-Path $bdir "build.ninja"))) {
            & meson setup $bdir $src --buildtype $buildtype -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false
            if ($LASTEXITCODE -ne 0) { throw "meson setup failed" }
        }
        & meson compile -C $bdir
        if ($LASTEXITCODE -ne 0) { throw "meson compile failed" }
    } finally { Pop-Location }
    $built = Join-Path $bdir "src\d3d9\d3d9.dll"
}

if (-not (Test-Path $built)) { throw "build finished but $built is missing" }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Copy-Item $built (Join-Path $outDir "dxvk_d3d9.dll") -Force
$pdb = [System.IO.Path]::ChangeExtension($built, ".pdb")
if (Test-Path $pdb) { Copy-Item $pdb (Join-Path $outDir "dxvk_d3d9.pdb") -Force }
"built: $outDir\dxvk_d3d9.dll ($buildtype)"
& (Join-Path $PSScriptRoot "dxvk-exports-check.ps1") (Join-Path $outDir "dxvk_d3d9.dll")
if ($LASTEXITCODE -ne 0) { throw "the fork is missing required exports" }
if ($Install) { & (Join-Path $PSScriptRoot "install.ps1") -DxvkDll (Join-Path $outDir "dxvk_d3d9.dll") }
