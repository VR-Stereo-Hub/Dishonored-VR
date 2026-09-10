# Compile and run the VR-30 yaw bookkeeping on the HOST - no game, no headset.
# The functions under test are sliced VERBATIM out of head_track.cpp, so this
# checks the shipped text rather than a re-implementation of it.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$src  = Join-Path $repo "src\game\dishonored\head_track.cpp"
$out  = Join-Path $repo "build\yawtest"
New-Item -ItemType Directory -Force -Path $out | Out-Null
python (Join-Path $PSScriptRoot "yawtest-slice.py") $src (Join-Path $out "yawtest_host.cpp")
if ($LASTEXITCODE -ne 0) { throw "Yaw source extraction failed." }
$root = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*" -Directory |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdk  = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" | Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib"     | Sort-Object Name -Descending | Select-Object -First 1).FullName
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB     = "$root\lib\x64;$libv\ucrt\x64;$libv\um\x64"
Push-Location $out
try {
  & "$root\bin\Hostx64\x64\cl.exe" /nologo /EHsc /W3 /Fe:yawtest_host.exe yawtest_host.cpp | Out-Null
  if ($LASTEXITCODE -ne 0) { throw "Yaw arithmetic test compilation failed." }
  .\yawtest_host.exe
  if ($LASTEXITCODE -ne 0) { throw "Yaw arithmetic tests failed." }
  & "$root\bin\Hostx64\x64\cl.exe" /nologo /EHsc /W3 /I. /Fe:yaw_owner_tests.exe (Join-Path $PSScriptRoot "yaw-owner-tests.cpp") | Out-Null
  if ($LASTEXITCODE -ne 0) { throw "Yaw ownership test compilation failed." }
  .\yaw_owner_tests.exe
  $code = $LASTEXITCODE
} finally { Pop-Location }
exit $code
