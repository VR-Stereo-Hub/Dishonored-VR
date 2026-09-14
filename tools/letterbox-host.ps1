# Compile and run letterbox query policy on the host. Never launches the game.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\letterbox-tests"
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
$env:LIB = "$root\lib\x86;$libv\ucrt\x86;$libv\um\x86"
Push-Location $out
try {
    $source = [IO.File]::ReadAllText((Join-Path $repo 'src\game\dishonored\cinematic_letterbox.cpp'))
    $patterns = [IO.File]::ReadAllText((Join-Path $repo 'src\game\dishonored\patterns.h'))
    $begin = $patterns.IndexOf('static const uintptr_t kLetterboxQuerySetup')
    $end = $patterns.IndexOf('// ---- Import table slots', $begin)
    $resultStart = $source.IndexOf('int LetterboxResult(')
    $resultEnd = $source.IndexOf('__declspec(noinline)', $resultStart)
    $fingerprintStart = $source.IndexOf('bool LetterboxFingerprint(')
    $fingerprintEnd = $source.IndexOf("`n}", $fingerprintStart) + 2
    if ($begin -lt 0 -or $end -lt 0 -or $resultStart -lt 0 -or $resultEnd -lt 0 -or $fingerprintStart -lt 0 -or $fingerprintEnd -lt 2) { throw 'Letterbox extraction markers changed.' }
    $extracted = $patterns.Substring($begin,$end-$begin) + $source.Substring($resultStart,$resultEnd-$resultStart) + $source.Substring($fingerprintStart,$fingerprintEnd-$fingerprintStart)
    [IO.File]::WriteAllText((Join-Path $out 'letterbox-extracted.h'),$extracted,[Text.UTF8Encoding]::new($false))
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /Fe:letterbox-host-tests.exe (Join-Path $PSScriptRoot 'letterbox-host-tests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Letterbox x86 compilation failed.' }
    .\letterbox-host-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Letterbox checks failed.' }
} finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
