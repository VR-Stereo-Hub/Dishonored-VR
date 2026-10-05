# Replay the AFW rebuild (VR-39) on a local `afw dump` capture and score it against the next native
# frame of each rebuilt eye. Never launches the game. The capture and the replay output are game
# output: they stay in the local dumps folder, never in the repository.
param([string]$Capture, [string]$Out = "", [switch]$Tint, [int]$Stereo = 1, [int]$Matrices = 1, [double]$Fg = 0, [double]$NearMiss = 6, [double]$OwnHands = 0, [int]$Mask = 1, [switch]$CompileOnly, [switch]$LegacyFreshWorld)
$ErrorActionPreference = "Stop"
if (-not $CompileOnly -and -not $Capture) { throw "Capture is required unless CompileOnly is selected." }
$repo = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $repo "build\afw-replay"
New-Item -ItemType Directory -Force -Path $bin | Out-Null
if (-not $CompileOnly -and -not $Out) { $Out = Join-Path $Capture "replay" }
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$root = Get-DvrMsvcRoot
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" |
        Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x86;$libv\ucrt\x86;$libv\um\x86"
Push-Location $bin
try {
    $legacyFlag = "/DDVR_WITH_LEGACY=" + [int][bool]$LegacyFreshWorld
    & "$root\bin\Hostx64\x86\cl.exe" $legacyFlag /nologo /EHsc /W4 /O2 /std:c++17 /I (Join-Path $repo "src") /Fe:afw-replay.exe (Join-Path $PSScriptRoot "afw-replay.cpp") (Join-Path $repo "src\core\gfx\afw_warp.cpp") /link d3d11.lib
    if ($LASTEXITCODE -ne 0) { throw "afw-replay compilation failed." }
    if ($CompileOnly) { return }
    .\afw-replay.exe $Capture $Out ([int][bool]$Tint) $Stereo $Matrices $Fg $NearMiss $OwnHands $Mask
    if ($LASTEXITCODE -ne 0) { throw "afw-replay failed." }
} finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
