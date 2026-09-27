# The DLSS transport on this PC's GPU with no game: builds the x64 helper and a 32-bit
# client (the proxy's own dlss_client.cpp) and runs tools\dlss-host-tests.cpp. Needs an
# NVIDIA RTX GPU and tools\fetch-ngx.ps1. Never launches the game.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([switch]$Cost, [switch]$Fsr)   # -Cost: also time every NGX preset at 2750x2850 output (slow); -Fsr: the FSR suite only
$ErrorActionPreference = "Stop"
if (Get-Process Dishonored -ErrorAction SilentlyContinue) { throw "Close the game before native GPU checks." }
$repo = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot "build-dlss-host.ps1")
$out = Join-Path $repo "build\dlss-host-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$root = Get-DvrMsvcRoot
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" | Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" | Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE; $savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x86;$libv\ucrt\x86;$libv\um\x86"
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I (Join-Path $repo "src") /Fe:dlss-host-tests.exe `
        (Join-Path $PSScriptRoot "dlss-host-tests.cpp") (Join-Path $repo "src\core\gfx\dlss_client.cpp") (Join-Path $repo "src\core\gfx\dlss_gpu.cpp") /link d3d11.lib dxgi.lib
    if ($LASTEXITCODE -ne 0) { throw "dlss host test compilation failed." }
    $data = Join-Path $out "data"
    if ($Fsr) { .\dlss-host-tests.exe (Join-Path $repo "build\dlss_host\dvr_dlss_host64.exe") $data --fsr }
    elseif ($Cost) { .\dlss-host-tests.exe (Join-Path $repo "build\dlss_host\dvr_dlss_host64.exe") $data --cost }
    else { .\dlss-host-tests.exe (Join-Path $repo "build\dlss_host\dvr_dlss_host64.exe") $data }
    $rc = $LASTEXITCODE
    Write-Host "helper log: $data\dlss_host.log"
    if ($rc -ne 0) { throw "dlss host tests failed." }
} finally {
    Pop-Location
    $env:INCLUDE = $savedInclude; $env:LIB = $savedLib
}
