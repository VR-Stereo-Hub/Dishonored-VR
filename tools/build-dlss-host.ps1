# Build the 64-bit DLSS helper (src\tools\dlss_host) with the x64 MSVC compiler.
# The mod itself is 32-bit only; this one executable is 64-bit because NVIDIA's NGX
# runtime is. Output: build\dlss_host\dvr_dlss_host64.exe plus a copy of the pinned
# nvngx_dlss.dll beside it. Needs tools\fetch-ngx.ps1 first.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$ngx = Join-Path $repo "third_party\ngx"
if (-not (Test-Path (Join-Path $ngx "include\nvsdk_ngx.h"))) { throw "NGX SDK missing: run tools\fetch-ngx.ps1" }
# FSR (the same helper, FidelityFX API): the headers and AMD's signed DLL from tools\fetch-ffx.ps1.
$ffx = Join-Path $repo "third_party\ffx"
if (-not (Test-Path (Join-Path $ffx "ffx-api\include\ffx_api\ffx_api.h"))) { throw "FidelityFX SDK missing: run tools\fetch-ffx.ps1" }
$out = Join-Path $repo "build\dlss_host"
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$root = Get-DvrMsvcRoot
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" | Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" | Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE; $savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x64;$libv\ucrt\x64;$libv\um\x64"
Push-Location $out
try {
    # NGX's nvsdk_ngx_d.lib is built /MD; mixing it with /MT duplicates CRT state (the fork's note).
    & "$root\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /W4 /MD /std:c++17 /DUNICODE /D_UNICODE `
        /I (Join-Path $repo "src") /I (Join-Path $ngx "include") /I (Join-Path $ffx "ffx-api\include") `
        (Join-Path $repo "src\tools\dlss_host\dlss_host.cpp") /Fe:dvr_dlss_host64.exe `
        /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup (Join-Path $ngx "lib\Windows_x86_64\x64\nvsdk_ngx_d.lib") user32.lib advapi32.lib
    if ($LASTEXITCODE -ne 0) { throw "dlss host compilation failed" }
    Copy-Item (Join-Path $ngx "lib\Windows_x86_64\rel\nvngx_dlss.dll") . -Force
    Copy-Item (Join-Path $ffx "PrebuiltSignedDLL\amd_fidelityfx_dx12.dll") . -Force
    Write-Host "build-dlss-host: $out\dvr_dlss_host64.exe"
} finally {
    Pop-Location
    $env:INCLUDE = $savedInclude; $env:LIB = $savedLib
}
