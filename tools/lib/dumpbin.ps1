# dumpbin.ps1 - locate the VS-bundled dumpbin and read a DLL's export names.
# Dot-sourced by exports-check.ps1 and dxvk-exports-check.ps1.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

function Get-DvrDumpbin {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install VS 2022 Build Tools." }
    $vs = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
    $d = Get-ChildItem "$vs\VC\Tools\MSVC\*\bin\Hostx64\x86\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $d) { $d = Get-ChildItem "$vs\VC\Tools\MSVC\*\bin\Hostx86\x86\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 }
    if (-not $d) { throw "dumpbin.exe not found under $vs - install the MSVC C++ build tools." }
    return $d.FullName
}

function Get-DvrExports {
    param([Parameter(Mandatory)][string]$Dll)
    if (-not (Test-Path $Dll)) { throw "not found: $Dll" }
    $out = & (Get-DvrDumpbin) /exports $Dll
    $names = @()
    $grab = $false
    foreach ($l in $out) {
        if ($l -match 'ordinal\s+hint') { $grab = $true; continue }
        if ($grab -and $l -match '^\s*Summary') { break }
        if ($grab -and $l -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)') { $names += $Matches[1] }
    }
    return $names
}
