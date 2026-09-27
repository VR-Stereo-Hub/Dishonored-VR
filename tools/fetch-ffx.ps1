# Fetch AMD's FidelityFX SDK (FSR 3.1 through the FidelityFX API) into third_party\ffx (gitignored,
# never committed): only the FFX API headers, the signed prebuilt amd_fidelityfx_dx12.dll and the
# license. Pinned to v1.1.4, commit c6efa6bf; the DLL is FileVersion 1.0.1.41314, Authenticode-signed
# by AMD, SHA256 12A5081257EC95B0B53AD51B4A87FB3C03F97FE0BBB59F9496968F8D50EF93A6.
# The SDK is MIT (third_party\ffx\LICENSE.txt); the DLL ships with the mod beside the DLSS helper.
# -Source: a local clone to copy from instead of GitHub (no network).
# FSR 4: the helper also loads SDK 2.x's amd_fidelityfx_loader_dx12.dll when it is placed beside it;
# that SDK is not fetched here.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([string]$Source = "https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git")
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $repo "third_party\ffx"
$tag = "v1.1.4"
$commit = "c6efa6bf7f2027b3ec94f28578bb5965eabb9e55"
$dllSha = "12A5081257EC95B0B53AD51B4A87FB3C03F97FE0BBB59F9496968F8D50EF93A6"
$stamp = Join-Path $dest ".ffx-commit"
if (-not (Test-Path $stamp)) {
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    $paths = @("LICENSE.txt", "ffx-api/include", "PrebuiltSignedDLL/amd_fidelityfx_dx12.dll")
    if (Test-Path (Join-Path $Source ".git")) {
        # A local clone: export just these paths at the pinned commit (a partial clone of a local path
        # fetches its blobs one by one).
        $tar = Join-Path $env:TEMP "dvr-ffx.tar"
        git -C $Source archive --format=tar -o $tar $commit @paths
        if ($LASTEXITCODE -ne 0) { throw "git archive of $commit from $Source failed (is it a FidelityFX SDK clone?)" }
        & (Join-Path $env:SystemRoot "System32\tar.exe") -x -f $tar -C $dest   # Windows tar: Git's GNU tar reads C: as a host
        if ($LASTEXITCODE -ne 0) { throw "extracting the FidelityFX files failed" }
        Remove-Item $tar
    } else {
        $tmp = Join-Path $env:TEMP "dvr-ffx-clone"
        if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
        git clone --depth 1 --branch $tag --filter=blob:none --sparse $Source $tmp
        if ($LASTEXITCODE -ne 0) { throw "git clone of the FidelityFX SDK failed" }
        git -C $tmp sparse-checkout set --no-cone /LICENSE.txt /ffx-api/include/ /PrebuiltSignedDLL/amd_fidelityfx_dx12.dll
        if ($LASTEXITCODE -ne 0) { throw "sparse checkout of the FidelityFX SDK failed" }
        $head = (git -C $tmp rev-parse HEAD).Trim()
        if ($head -ne $commit) { throw "the FidelityFX SDK $tag is at $head, expected $commit" }
        Copy-Item -Recurse (Join-Path $tmp "ffx-api") $dest -Force
        New-Item -ItemType Directory -Force -Path (Join-Path $dest "PrebuiltSignedDLL") | Out-Null
        Copy-Item (Join-Path $tmp "PrebuiltSignedDLLmd_fidelityfx_dx12.dll") (Join-Path $dest "PrebuiltSignedDLL") -Force
        Copy-Item (Join-Path $tmp "LICENSE.txt") $dest -Force
        Remove-Item -Recurse -Force $tmp
    }
    Set-Content -Path $stamp -Value $commit -Encoding ascii
}
if ((Get-Content $stamp -TotalCount 1).Trim() -ne $commit) { throw "third_partyfx holds $((Get-Content $stamp -TotalCount 1)), expected $commit ($tag)" }
$dll = Join-Path $dest "PrebuiltSignedDLL\amd_fidelityfx_dx12.dll"
$sha = (Get-FileHash $dll -Algorithm SHA256).Hash
if ($sha -ne $dllSha) { throw "amd_fidelityfx_dx12.dll hash $sha does not match the pinned $dllSha" }
Write-Host "fetch-ffx: FidelityFX SDK $tag at $commit, amd_fidelityfx_dx12.dll $sha"
