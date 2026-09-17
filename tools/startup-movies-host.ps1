# Compile production startup command-line functions against clean config fixtures.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo "build\startup-movies-tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$text=[IO.File]::ReadAllText((Join-Path $repo 'src/core/window/render_size.cpp'))
foreach($pair in @(@('LaunchArgsBuild','startup_build.inc'),@('LaunchArgsResolveFromIni','startup_resolve.inc'))) {
    $body=[regex]::Match($text,('(?ms)^static void '+$pair[0]+'\([^;\r\n]*\)\r?\n\{.*?^\}'))
    if(-not $body.Success){throw 'Production startup function extraction failed'}
    [IO.File]::WriteAllText((Join-Path $out $pair[1]),$body.Value)
}
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
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 /I. /I (Join-Path $repo "src") /Fe:startup-movies-tests.exe (Join-Path $PSScriptRoot "startup-movies-tests.cpp")
    if ($LASTEXITCODE -ne 0) { throw "startup-movies compilation failed." }
    .\startup-movies-tests.exe
    if ($LASTEXITCODE -ne 0) { throw "startup-movies tests failed." }
 } finally {
    Pop-Location
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
}
