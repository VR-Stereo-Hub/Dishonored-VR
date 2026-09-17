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
        & java -jar $FfdecJar -export script,frame $preview $_.FullName
        if ($LASTEXITCODE -ne 0) { throw 'FFDec preview export failed' }
        & java -jar $FfdecJar -swf2xml $_.FullName (Join-Path $out "$package.xml")
        if ($LASTEXITCODE -ne 0) { throw 'FFDec structure export failed' }
    }
}
Get-FileHash -Algorithm SHA256 -LiteralPath $Umodel,$FfdecJar | ConvertTo-Json | Set-Content (Join-Path $out 'tool-hashes.json')
Write-Host "Local HUD inspection files: $out"
Write-Host 'Static authoring frames do not execute runtime item population or safe-area layout.'
