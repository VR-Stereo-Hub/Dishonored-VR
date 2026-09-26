# Compile and run the production clarity shaders (resolve, temporal, sharpen) on the host. Never launches the game.
$ErrorActionPreference = "Stop"
if (Get-Process Dishonored -ErrorAction SilentlyContinue) { throw "Close game before native GPU checks." }
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\clarity-gpu-tests"
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I (Join-Path $repo "src") /Fe:clarity-gpu-tests.exe (Join-Path $PSScriptRoot "clarity-gpu-tests.cpp") (Join-Path $repo "src\core\gfx\clarity_gpu.cpp") /link d3d11.lib
    if ($LASTEXITCODE -ne 0) { throw "clarity-gpu compilation failed." }
    .\clarity-gpu-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "clarity-gpu tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
