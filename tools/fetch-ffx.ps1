# Fetch AMD's FidelityFX SDK (FSR through the FidelityFX API) into third_party\ffx (gitignored, never
# committed): only the API headers, the two signed prebuilt DLLs the helper loads and the license.
#
# Pinned to v2.3.0, commit 60f4ea81. amd_fidelityfx_loader_dx12.dll is the API entry point; it loads
# amd_fidelityfx_upscaler_dx12.dll, the provider that offers FSR 4 on the AMD GPUs that run it (RDNA 4)
# and FSR 3.1 everywhere else. Both are Authenticode-signed by AMD and hash-pinned below. MIT
# (third_party\ffx\LICENSE.md). The helper also still accepts SDK 1.1.x's amd_fidelityfx_dx12.dll.
# -Source: a local clone to export from instead of GitHub (no network).
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([string]$Source = "https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git")
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $repo "third_party\ffx"
$tag = "v2.3.0"
$commit = "60f4ea81909200d8542eca14dccb2628b763a9a3"
$pins = @{
    "amd_fidelityfx_loader_dx12.dll"   = "E2D85AA05A9BD9ED8B38935FDF5199372CCA6F74C12015143BB6F945EE1608AA"
    "amd_fidelityfx_upscaler_dx12.dll" = "D0DCCCC74A43C44BA435B7A369B456E0970D8A4464E4BD683119B374F2C9FB46"
}
$paths = @("docs/license.md", "Kits/FidelityFX/api/include", "Kits/FidelityFX/upscalers/include",
           "Kits/FidelityFX/signedbin/amd_fidelityfx_loader_dx12.dll", "Kits/FidelityFX/signedbin/amd_fidelityfx_upscaler_dx12.dll")
$stamp = Join-Path $dest ".ffx-commit"
if (-not ((Test-Path $stamp) -and (Get-Content $stamp -TotalCount 1).Trim() -eq $commit)) {
    if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    $src = $Source
    $tmp = $null
    if ($Source -match '^[a-z]+://' -or -not (Test-Path (Join-Path $Source ".git"))) {
        # GitHub: a shallow, blobless, sparse clone of the tag; only the listed files are downloaded.
        $tmp = Join-Path $env:TEMP "dvr-ffx-clone"
        if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
        git clone --quiet --depth 1 --branch $tag --filter=blob:none --no-checkout $Source $tmp
        if ($LASTEXITCODE -ne 0) { throw "git clone of the FidelityFX SDK failed" }
        git -C $tmp sparse-checkout set --no-cone @($paths | ForEach-Object { "/$_" })
        git -C $tmp checkout --quiet $commit
        if ($LASTEXITCODE -ne 0) { throw "checkout of $commit ($tag) failed" }
        $src = $tmp
    }
    $head = (git -C $src rev-parse $commit 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "$src does not hold commit $commit ($tag)" }
    $tar = Join-Path $env:TEMP "dvr-ffx.tar"
    git -C $src archive --format=tar -o $tar $commit @paths
    if ($LASTEXITCODE -ne 0) { throw "git archive of $commit failed" }
    & (Join-Path $env:SystemRoot "System32\tar.exe") -x -f $tar -C $dest   # Windows tar: Git's GNU tar reads C: as a host
    if ($LASTEXITCODE -ne 0) { throw "extracting the FidelityFX files failed" }
    Remove-Item $tar
    if ($tmp) { Remove-Item -Recurse -Force $tmp }
    Copy-Item (Join-Path $dest "docs\license.md") (Join-Path $dest "LICENSE.md") -Force
    Set-Content -Path $stamp -Value $commit -Encoding ascii
}
foreach ($name in $pins.Keys) {
    $dll = Join-Path $dest "Kits\FidelityFX\signedbin\$name"
    $sha = (Get-FileHash $dll -Algorithm SHA256).Hash
    if ($sha -ne $pins[$name]) { throw "$name hash $sha does not match the pinned $($pins[$name])" }
    if ((Get-AuthenticodeSignature $dll).Status -ne "Valid") { throw "$name is not validly signed" }
}
Write-Host "fetch-ffx: FidelityFX SDK $tag at $commit, loader + upscaler DLLs hash-verified and signed"
