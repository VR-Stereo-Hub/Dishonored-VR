# Compile and run cinematic camera math on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\cinematic-head-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$root = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*" -Directory |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdk = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" |
        Sort-Object Name -Descending | Select-Object -First 1).FullName
$libv = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" |
         Sort-Object Name -Descending | Select-Object -First 1).FullName
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
$env:INCLUDE = "$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB = "$root\lib\x64;$libv\ucrt\x64;$libv\um\x64"
Push-Location $out
try {
    & "$root\bin\Hostx64\x64\cl.exe" /nologo /EHsc /W4 /std:c++17 /Fe:cinematic-head-tests.exe (Join-Path $PSScriptRoot "cinematic-head-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "Cinematic head math compilation failed." }
    .\cinematic-head-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "Cinematic head math tests failed." }
    # Extract the exact production declarations and bodies, so the fixture tests
    # shipped code rather than a second implementation of restoration behavior.
    $cameraText = [IO.File]::ReadAllText((Join-Path $repo 'src\game\dishonored\camera.cpp'))
    $writerStart = $cameraText.IndexOf('struct Writer {')
    $scopeEnd = $cameraText.IndexOf('// ---- positional tracking', $writerStart)
    $beginStart = $cameraText.IndexOf('bool begin_view_scope(')
    $beginEnd = $cameraText.IndexOf('// ---- the writer (script lane)', $beginStart)
    if ($writerStart -lt 0 -or $scopeEnd -lt 0 -or $beginStart -lt 0 -or $beginEnd -lt 0) {
        throw 'Cinematic scope extraction markers changed; update the fixture.'
    }
    $extracted = $cameraText.Substring($writerStart,$scopeEnd-$writerStart) + $cameraText.Substring($beginStart,$beginEnd-$beginStart)
    [IO.File]::WriteAllText((Join-Path $out 'cinematic-scope-extracted.h'),$extracted,[Text.UTF8Encoding]::new($false))
    & "$root\bin\Hostx64\x64\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /Fe:cinematic-scope-tests.exe (Join-Path $PSScriptRoot 'cinematic-scope-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Cinematic scope test compilation failed.' }
    .\cinematic-scope-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Cinematic scope tests failed.' }
} finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
