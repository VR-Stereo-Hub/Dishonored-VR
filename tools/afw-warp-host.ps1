# Compile and run the AFW held-eye warp (VR-39) on the host. Never launches the game.
param([switch]$OldMswControl)
$ErrorActionPreference = "Stop"
if (Get-Process Dishonored -ErrorAction SilentlyContinue) { throw "Close game before native GPU checks." }
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\afw-warp-tests"
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
    $warpSource = Join-Path $repo 'src\core\gfx\afw_warp.cpp'
    if ($OldMswControl) {
        $old = (git -C $repo show '11dcf7db9:src/core/gfx/afw_warp.cpp') -join "`n"
        if ($LASTEXITCODE -ne 0) { throw 'Cannot read accepted build 242 control' }
        # Only adapt the function signature. Build 242 ignores the new display-time argument.
        $old = $old.Replace('const HandPose* slotHands) {', 'const HandPose* slotHands, int64_t /*displayTime*/) {')
        $warpSource = Join-Path $out 'afw_warp_242.cpp'
        [IO.File]::WriteAllText($warpSource, $old, [Text.UTF8Encoding]::new($false))
    }
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I (Join-Path $repo "src") /Fe:afw-warp-tests.exe (Join-Path $PSScriptRoot "afw-warp-tests.cpp") $warpSource /link d3d11.lib
    if ($LASTEXITCODE -ne 0) { throw "afw-warp compilation failed." }
    .\afw-warp-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "afw-warp tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
