# VR-198: build and run the installer's unit tests (tools\installer-tests.cpp)
# against src\tools\installer\sys and model with the plain cl.exe the other
# *-host.ps1 suites use. No game, no headset, no ImGui.
#   .\tools\installer-host.ps1
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param()
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\installer-tests'
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$vc = Get-DvrMsvcRoot
$sdkInc = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$inst = Join-Path $repo 'src\tools\installer'
$oldInc = $env:INCLUDE; $oldLib = $env:LIB
Push-Location $out
try {
    $env:INCLUDE = "$vc\include;$sdkInc\ucrt;$sdkInc\shared;$sdkInc\um"
    $env:LIB = "$vc\lib\x86;$sdkLib\ucrt\x86;$sdkLib\um\x86"
    $srcs = @(
        (Join-Path $PSScriptRoot 'installer-tests.cpp'),
        "$inst\sys\fs.cpp", "$inst\sys\game_ini.cpp", "$inst\sys\profile.cpp", "$inst\sys\steam.cpp",
        "$inst\sys\process.cpp", "$inst\sys\install_record.cpp", "$inst\model\choices.cpp")
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I $inst $srcs `
        /Fe:launcher_checks.exe /link bcrypt.lib shell32.lib ole32.lib oleaut32.lib advapi32.lib user32.lib uuid.lib
    if ($LASTEXITCODE -ne 0) { throw 'installer-tests compilation failed' }
    & .\launcher_checks.exe
    $rc = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $oldInc; $env:LIB = $oldLib
}
if ($rc -ne 0) { throw "installer-tests failed ($rc)" }
'installer-tests: all passed'
