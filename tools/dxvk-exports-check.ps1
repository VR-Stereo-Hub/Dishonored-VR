# dxvk-exports-check.ps1 - the DXVK fork must export every dxvk_vr_* symbol the
# proxy resolves by GetProcAddress (dxvk\DISHONORED-FORK.md, export contract).
#   .\tools\dxvk-exports-check.ps1 build\dxvk\dxvk_d3d9.dll
# tests\golden\dxvk-vr-exports.txt lists the required names; a name suffixed
# with " optional" is reported but does not fail (dxvk_vr_view: see the fork
# doc - the published patch series never had it).
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([Parameter(Mandatory)][string]$Dll)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "lib\dumpbin.ps1")

$required = @(); $optional = @()
foreach ($l in Get-Content (Join-Path $repo "tests\golden\dxvk-vr-exports.txt")) {
    if (-not $l -or $l.StartsWith("#")) { continue }
    $parts = $l.Trim() -split '\s+'
    if ($parts.Count -gt 1 -and $parts[1] -eq "optional") { $optional += $parts[0] } else { $required += $parts[0] }
}
$have = Get-DvrExports $Dll
$missing = @($required | Where-Object { $have -notcontains $_ })
$missingOpt = @($optional | Where-Object { $have -notcontains $_ })
$extra = @($have | Where-Object { $_ -like 'dxvk_vr_*' -and $required -notcontains $_ -and $optional -notcontains $_ })
"fork exports: $(@($have | Where-Object { $_ -like 'dxvk_vr_*' }).Count) dxvk_vr_* names in $Dll"
if ($missingOpt) { "optional, absent: $($missingOpt -join ', ')" }
if ($extra) { "extra (unused by the proxy): $($extra -join ', ')" }
if ($missing) { "MISSING REQUIRED: $($missing -join ', ')"; exit 1 }
"fork exports OK: all $($required.Count) required names present"
exit 0
