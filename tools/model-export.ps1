# model-export.ps1 - pull meshes, animations and textures out of the game's cooked packages
# with UModel (UE Viewer), into the LOCAL model workspace. Never touches the game install.
#
#   .\tools\model-export.ps1 -List Startup -Grep "AnimSet|SkeletalMesh"   # what a package holds
#   .\tools\model-export.ps1 -Find "Skm_Player|Ply_.*_as"                 # which package holds it (indexes once)
#   .\tools\model-export.ps1 -Package Engine -Object Skm_Player           # mesh (PSK) + its textures (TGA)
#   .\tools\model-export.ps1 -Package Startup -Object Ply_Generic_as      # AnimSet -> PSA (every sequence)
#   .\tools\model-export.ps1 -Package Engine -Object Skm_Player -Format gltf
#
# Output: <model_workspace>\originals\<Package>\<Class>\<Object>.<ext> - UModel's layout.
# originals\ is the untouched extraction; edit copies in working\, write results to
# exports\. Everything here is game-derived: it stays out of the repo (docs/MODEL_WORKFLOW.md).
# PSK coordinates are UModel's export frame, which REFLECTS Y relative to the engine's
# mesh space (ARM_IK.md, ENGINE_NOTES) - anything that goes back to runtime must undo it.
param(
    [string]$Package = "",
    [string]$Object = "",
    [ValidateSet('psk','gltf','md5')][string]$Format = 'psk',
    [switch]$NoTextures,
    [switch]$NoAnim,
    [string]$List = "",
    [string]$Grep = "",
    [string]$Find = "",
    [switch]$Reindex
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\tool-paths.ps1")
$umodel = Get-DvrTool umodel
$cooked = Get-DvrTool cooked_dir
$ws     = Get-DvrTool model_workspace
# Native tools write progress to stderr; under 'Stop' PowerShell 5.1 turns each line
# into a terminating error. Every native call below checks $LASTEXITCODE instead.
$ErrorActionPreference = 'Continue'

if ($List) {
    $lines = & $umodel -list "-path=$cooked" $List 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "umodel -list $List failed (exit $LASTEXITCODE)" }
    if ($Grep) { $lines = $lines | Where-Object { $_ -match $Grep } }
    $lines | Write-Output
    exit 0
}

if ($Find) {
    # One `umodel -list` per package is slow (hundreds of packages), so the inventory is
    # cached per package and reused until the package file changes.
    $idx = Join-Path $ws "index"
    New-Item -ItemType Directory -Force -Path $idx | Out-Null
    $hits = 0; $pkgs = @(Get-ChildItem -LiteralPath $cooked -Filter *.upk)
    Write-Output "searching $($pkgs.Count) packages for /$Find/ (index: $idx)"
    foreach ($p in $pkgs) {
        $cache = Join-Path $idx ($p.BaseName + ".txt")
        if ($Reindex -or -not (Test-Path -LiteralPath $cache) -or (Get-Item -LiteralPath $cache).LastWriteTime -lt $p.LastWriteTime) {
            & $umodel -list "-path=$cooked" $p.BaseName 2>&1 | ForEach-Object { "$_" } | Set-Content -LiteralPath $cache -Encoding utf8
        }
        foreach ($l in (Get-Content -LiteralPath $cache)) {
            if ($l -match '^\s*\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\w+)\s+(\S+)' -and ($Matches[2] -match $Find) -and $Matches[1] -notin @('Class','ObjectProperty','NameProperty','StructProperty')) {
                Write-Output ("{0,-34} {1,-16} {2}" -f $p.BaseName, $Matches[1], $Matches[2]); $hits++
            }
        }
    }
    Write-Output "$hits match(es)"
    exit 0
}

if (-not $Package) { throw "Name -Package (and usually -Object), or use -List / -Find. See docs\MODEL_WORKFLOW.md." }
if (-not $Object) { Write-Warning "No -Object: exporting the WHOLE package $Package (can be large)." }
$out = Join-Path $ws "originals"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$args2 = @('-export', "-path=$cooked", "-out=$out", "-$Format")
if ($NoTextures) { $args2 += '-notex' }
if ($NoAnim) { $args2 += '-noanim' }
$args2 += $Package
if ($Object) { $args2 += $Object }
$before = Get-Date
$log = & $umodel @args2 2>&1 | ForEach-Object { "$_" }
$log | Write-Output
if ($LASTEXITCODE -ne 0) { throw "umodel export failed (exit $LASTEXITCODE)" }
$new = @(Get-ChildItem -LiteralPath $out -Recurse -File | Where-Object { $_.LastWriteTime -ge $before.AddSeconds(-2) })
if ($new.Count -eq 0) { throw "umodel exited 0 but wrote nothing under $out - wrong -Object name or class (check -List $Package)." }
Write-Output ""
Write-Output "wrote $($new.Count) file(s):"
$new | ForEach-Object { "  {0}  ({1:N0} bytes)" -f $_.FullName, $_.Length } | Write-Output
