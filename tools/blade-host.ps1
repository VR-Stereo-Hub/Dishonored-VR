# Compile and run the held sword's blade maths (VR-173) on the host. Never launches the game.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\blade-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++20 /I. /I (Join-Path $repo "src") /Fe:blade-tests.exe (Join-Path $PSScriptRoot "blade-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "blade maths compilation failed." }
    .\blade-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "blade maths tests failed." }
} finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}

# The blade is NOT an aim-ray candidate. The bolt reader's own refusal of weapon bodies
# has to stay: a sword aimed from would put every shot along the blade.
$bolt = Get-Content (Join-Path $repo "src\game\dishonored\hands\bolt_model_ray.cpp") -Raw
if ($bolt -match 'Sword') { throw "bolt_model_ray.cpp names the sword: the blade must not enter the aim ray's measurement." }
$attach = Get-Content (Join-Path $repo "src\game\dishonored\hands\weapon_attach.cpp") -Raw
if ($attach.IndexOf('blade_axis.cpp') -lt $attach.IndexOf('bolt_model_ray.cpp')) { throw "blade_axis.cpp must be included after bolt_model_ray.cpp." }
"blade: the aim ray's candidate list is untouched"
