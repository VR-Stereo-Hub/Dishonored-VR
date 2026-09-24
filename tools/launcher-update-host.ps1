# VR-214: build and run the launcher update and discovery tests
# against src\tools\installer\sys and model with the plain cl.exe the other
# *-host.ps1 suites use. No game, no headset, no ImGui.
#   .\tools\installer-host.ps1
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([switch]$Live)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\launcher-update-tests'
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$vc = Get-DvrMsvcRoot
$sdkInc = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkLib = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$json = Join-Path $repo "third_party\OpenXR-SDK\src\external\jsoncpp"
$inst = Join-Path $repo 'src\tools\installer'
$oldInc = $env:INCLUDE; $oldLib = $env:LIB
Push-Location $out
try {
    $env:INCLUDE = "$vc\include;$sdkInc\ucrt;$sdkInc\shared;$sdkInc\um"
    $env:LIB = "$vc\lib\x86;$sdkLib\ucrt\x86;$sdkLib\um\x86"
    $srcs = @(
        (Join-Path $PSScriptRoot 'launcher-update-tests.cpp'),
        "$inst\sys\fs.cpp", "$inst\sys\game_ini.cpp", "$inst\sys\profile.cpp", "$inst\sys\steam.cpp",
        "$inst\sys\process.cpp", "$inst\sys\install_record.cpp", "$inst\model\choices.cpp",
        "$inst\sys\updates.cpp", "$inst\sys\discovery.cpp",
        "$json\src\lib_json\json_reader.cpp", "$json\src\lib_json\json_value.cpp", "$json\src\lib_json\json_writer.cpp")
    & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /W3 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I $inst /I "$json\include" $srcs `
        /Fe:launcher_checks.exe /link bcrypt.lib shell32.lib ole32.lib oleaut32.lib advapi32.lib user32.lib uuid.lib winhttp.lib version.lib
    if ($LASTEXITCODE -ne 0) { throw 'launcher-update-tests compilation failed' }
    $versionText=Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
    if($versionText -notmatch 'project\(DishonoredVR VERSION ([0-9.]+)') { throw 'Cannot read version' }
    $version=$Matches[1]
    $launcher = Join-Path $repo "build\src\RelWithDebInfo\DishonoredVR-Launcher-v$version.exe"
    if ($Live) { & .\launcher_checks.exe $launcher $version --live } else { & .\launcher_checks.exe $launcher $version }
    $rc = $LASTEXITCODE
} finally {
    Pop-Location
    $env:INCLUDE = $oldInc; $env:LIB = $oldLib
}
if ($rc -ne 0) { throw "launcher-update-tests failed ($rc)" }
'launcher-update-tests: all passed'
