# Actual launcher widgets with synthetic pointer events. No game or file operations.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo 'build\installer-ui-tests'
New-Item -ItemType Directory -Force -Path $out | Out-Null
. (Join-Path $PSScriptRoot 'lib\msvc.ps1')
$vc=Get-DvrMsvcRoot
$sdkInc=(Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkLib=(Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$inst=Join-Path $repo 'src\tools\installer'
$imgui=Join-Path $repo 'third_party\imgui'
$json=Join-Path $repo 'third_party\OpenXR-SDK\src\external\jsoncpp'
$oldInc=$env:INCLUDE;$oldLib=$env:LIB
Push-Location $out
try {
 $env:INCLUDE="$vc\include;$sdkInc\ucrt;$sdkInc\shared;$sdkInc\um;$sdkInc\winrt"
 $env:LIB="$vc\lib\x86;$sdkLib\ucrt\x86;$sdkLib\um\x86"
 $srcs=@("$PSScriptRoot\installer-ui-tests.cpp","$inst\ui\screens.cpp","$inst\ui\widgets.cpp","$inst\model\fake_states.cpp","$inst\model\choices.cpp","$inst\sys\resources.cpp","$inst\sys\fs.cpp","$inst\sys\process.cpp","$inst\sys\discovery.cpp","$inst\sys\steam.cpp","$inst\sys\updates.cpp","$repo\src\core\ui\ovl_ui.cpp","$json\src\lib_json\json_reader.cpp","$json\src\lib_json\json_value.cpp","$json\src\lib_json\json_writer.cpp","$imgui\imgui.cpp","$imgui\imgui_draw.cpp","$imgui\imgui_tables.cpp","$imgui\imgui_widgets.cpp")
 & "$vc\bin\Hostx64\x86\cl.exe" /nologo /std:c++20 /EHsc /O1 /W3 /DIMGUI_ENABLE_TEST_ENGINE /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I $inst /I "$repo\src" /I $imgui /I "$json\include" $srcs /Fe:launcher_ui_tests.exe /link bcrypt.lib shell32.lib ole32.lib oleaut32.lib advapi32.lib user32.lib uuid.lib winhttp.lib version.lib windowscodecs.lib d3d11.lib gdi32.lib
 if($LASTEXITCODE -ne 0){throw 'UI host compile failed'}
 & .\launcher_ui_tests.exe
 if($LASTEXITCODE -ne 0){throw 'UI host checks failed'}
} finally {Pop-Location;$env:INCLUDE=$oldInc;$env:LIB=$oldLib}
