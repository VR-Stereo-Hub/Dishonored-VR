# lib/msvc.ps1 - where the MSVC toolset is on THIS machine, for the host test
# scripts (tools/*-host.ps1). Dot-source it and call Get-DvrMsvcRoot.
#
# The host scripts used to glob "C:\Program Files\Microsoft Visual Studio",
# which is where a full Visual Studio installs; the VS 2022 BUILD TOOLS (what
# tools/build.ps1 already finds through vswhere) live under Program Files (x86),
# and on such a machine every host suite threw before compiling anything.
# vswhere first, the old glob as the fallback, and a clear error naming both.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
function Get-DvrMsvcRoot {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $root = $null
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
        if ($vs) {
            $root = (Get-ChildItem "$vs\VC\Tools\MSVC\*" -Directory -ErrorAction SilentlyContinue |
                     Sort-Object Name -Descending | Select-Object -First 1).FullName
        }
    }
    if (-not $root) {
        $root = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*" -Directory -ErrorAction SilentlyContinue |
                 Sort-Object Name -Descending | Select-Object -First 1).FullName
    }
    if (-not $root) {
        throw "MSVC toolset not found: neither vswhere ($vswhere) nor C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC has one. Install the VS 2022 Build Tools C++ workload."
    }
    return $root
}
