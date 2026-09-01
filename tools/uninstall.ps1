# Removes the mod from the game folder; restores a backed-up d3d9.dll if one
# exists. Touches only files we put there; leaves dishonored_vr.ini in place.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([string]$GamePath = "")

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$GamePath = Get-DvrGamePath $GamePath

$proxy = Join-Path $GamePath "d3d9.dll"
$backup = Join-Path $GamePath "d3d9.dll.dvr-backup"
$ours = (Test-Path (Join-Path $GamePath "dishonored_vr.log")) -or (Test-Path (Join-Path $GamePath "dishonored_vr.ini"))

if (Test-Path $proxy) {
    if ($ours -or (Test-Path (Join-Path $GamePath "dxvk_d3d9.dll"))) {
        Remove-Item $proxy -Force
        Write-Host "Removed d3d9.dll (our proxy)"
    } else {
        Write-Host "d3d9.dll present but no trace of the mod beside it - refusing to delete a proxy that may not be ours."
    }
}
foreach ($n in @("dxvk_d3d9.dll", "openvr_api.dll")) {
    $p = Join-Path $GamePath $n
    if (Test-Path $p) { Remove-Item $p -Force; Write-Host "Removed $n" }
}
if (Test-Path $backup) {
    Move-Item $backup $proxy -Force
    Write-Host "Restored original d3d9.dll from backup"
}
Write-Host "Done. dishonored_vr.ini was left in place (delete it to reset settings)."
