# Compile and run the semantic HUD ownership transport on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\hud-owner-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/hud_owner.cpp'))
$elements=[regex]::Match($text,'(?ms)^const int elements\[32\]=\{.*?;')
if(-not $elements.Success){throw 'Native clip table extraction failed'}
$snippets=@($elements.Value)
foreach($name in @('Character','Value','MovieView','SpriteMovie','RefreshQuickMovie','QuickPotionOwner','Display','Publish','Execute','ExecuteStub')) {
    $pattern='(?ms)^(?:int|Owner|uintptr_t|const uint8_t\*|void __fastcall|uint32_t __cdecl|__declspec\(naked\) void) '+$name+'\(.*?^\}'
    $body=[regex]::Match($text,$pattern)
    if(-not $body.Success){throw "Dispatch extraction failed: $name"}
    $snippets+=$body.Value
}
[IO.File]::WriteAllText((Join-Path $out 'hud_owner_dispatch.inc'),($snippets -join "`n"))
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
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /O2 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:hud-owner-tests.exe (Join-Path $PSScriptRoot "hud-owner-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "hud-owner compilation failed." }
    .\hud-owner-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "hud-owner tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
