# Compile and run the bounded HUD identity probes on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\hud-identity-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/core/gfx/hud_class.cpp'))
$body=[regex]::Match($text,'(?ms)^void owner_trace\(.*?^\}')
if(-not $body.Success){throw 'Ownership trace extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_owner_trace.inc'),$body.Value)
$native=[IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/objective_markers.cpp'))
$probe=[regex]::Match($native,'(?ms)^void MarkerIdentityTrace\(.*?^\}')
if(-not $probe.Success){throw 'Native ownership probe extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_native_identity.inc'),$probe.Value)
$patterns=[IO.File]::ReadAllText((Join-Path $repo 'src/game/dishonored/patterns.h'))
$fields=[regex]::Matches($patterns,'(?m)^static const uint32_t k(MarkerGfxInterface|GfxResolvedCharacter)=.*$')
if($fields.Count -ne 2){throw 'Native ownership field extraction failed'}
[IO.File]::WriteAllText((Join-Path $out 'hud_identity_fields.inc'),(($fields | ForEach-Object {$_.Value}) -join "`n"))
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /O2 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:hud-identity-tests.exe (Join-Path $PSScriptRoot "hud-identity-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "hud-identity compilation failed." }
    .\hud-identity-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "hud-identity tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
