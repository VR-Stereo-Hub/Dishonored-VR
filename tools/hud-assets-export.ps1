# Offline HUD inspection only. Never launches the game or changes its packages.
param(
    [Parameter(Mandatory=$true)][string]$CookedDir,
    [string]$Umodel = "$PSScriptRoot\..\build\hud-assets\tools\umodel.exe",
    [string]$FfdecJar = "$PSScriptRoot\..\build\hud-assets\tools\ffdec\ffdec.jar"
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\hud-assets'
# Official tool documentation: https://www.gildor.org/projects/umodel/faq
# FFDec documentation/downloads: https://www.free-decompiler.com/flash/
# Tools and extracted game material are local, ignored build dependencies.
foreach ($file in @($Umodel,$FfdecJar)) { if (-not (Test-Path -LiteralPath $file)) { throw "Missing offline tool: $file" } }
New-Item -ItemType Directory -Force -Path $out | Out-Null
# The wheel imports common_assets/lib.swf. In the shipped cooked build its
# movie and textures are folded into Startup.upk, not a Common_assets package.
& $Umodel -list "-path=$CookedDir" Startup | Set-Content (Join-Path $out 'startup-list.log')
if ($LASTEXITCODE -ne 0) { throw 'Startup inventory failed' }
& $Umodel -export -3rdparty -noanim -nomesh -nostat "-path=$CookedDir" "-out=$out\export" Startup lib
if ($LASTEXITCODE -ne 0) { throw 'Shared library export failed' }
$library = Join-Path $out 'export\Startup\SwfMovie\lib.gfx'
& java -jar $FfdecJar -swf2xml $library (Join-Path $out 'lib.xml')
if ($LASTEXITCODE -ne 0) { throw 'Shared library XML failed' }
[xml]$structure = Get-Content -LiteralPath (Join-Path $out 'lib.xml') -Raw
$images = $structure.SelectNodes('//item[@type="DefineExternalImage2"]')
foreach ($image in $images) {
    $name = [IO.Path]::GetFileNameWithoutExtension($image.fileName)
    & $Umodel -export -3rdparty -noanim -nomesh -nostat "-path=$CookedDir" "-out=$out\export" Startup $name
    if ($LASTEXITCODE -ne 0) { throw "Shared texture export failed: $name" }
    Copy-Item -LiteralPath (Join-Path $out "export\Startup\Texture2D\$name.tga") -Destination (Split-Path $library)
}
# EquipmentIcon.SetIconImage calls req_EquipmentIconImage in the engine.
# The itemIcons timeline is the shell; these separate textures are its artwork.
$inventory = Get-Content -LiteralPath (Join-Path $out 'startup-list.log') -Raw
$icons = [regex]::Matches($inventory,'Texture2D (ic_(?:item|pow)_\w+)')
if ($icons.Count -eq 0) { throw 'No runtime equipment icons in Startup inventory' }
foreach ($match in $icons) {
    $name = $match.Groups[1].Value
    & $Umodel -export -png -noanim -nomesh -nostat "-path=$CookedDir" "-out=$out\export" Startup $name
    if ($LASTEXITCODE -ne 0) { throw "Equipment icon export failed: $name" }
}
& java -jar $FfdecJar -export script (Join-Path $out 'library-scripts') $library
if ($LASTEXITCODE -ne 0) { throw 'Shared library script export failed' }
foreach ($package in @('UI_PowerWheel_SF','UI_HUD_SF')) {
    if (-not (Test-Path -LiteralPath (Join-Path $CookedDir "$package.upk"))) { throw "Package missing: $package" }
    & $Umodel -export -3rdparty -noanim -nomesh -nostat "-path=$CookedDir" "-out=$out\export" $package
    if ($LASTEXITCODE -ne 0) { throw "UModel export failed: $package" }
    $packageOut = Join-Path $out "export\$package"
    $movieDir = Join-Path $packageOut 'SwfMovie'
    # GFx references external TGA files beside the movie; without them the
    # authoring preview contains red placeholders, not the actual artwork.
    Get-ChildItem -LiteralPath (Join-Path $packageOut 'Texture2D') -Filter '*.tga' | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $movieDir
    }
    Get-ChildItem -LiteralPath $movieDir -Filter '*.gfx' | ForEach-Object {
        $preview = Join-Path $out "preview-$package"
        & java -jar $FfdecJar -importAssets yes,local -changeImport "../common_assets/lib.swf" $library -export script,frame $preview $_.FullName
        if ($LASTEXITCODE -ne 0) { throw 'FFDec preview export failed' }
        & java -jar $FfdecJar -swf2xml $_.FullName (Join-Path $out "$package.xml")
        if ($LASTEXITCODE -ne 0) { throw 'FFDec structure export failed' }
    }
}
Get-FileHash -Algorithm SHA256 -LiteralPath $Umodel,$FfdecJar | ConvertTo-Json | Set-Content (Join-Path $out 'tool-hashes.json')
Write-Host "Local HUD inspection files: $out"
Write-Host 'Shared library and runtime icon textures included. Static frames do not execute engine callbacks, item population or safe-area layout.'
