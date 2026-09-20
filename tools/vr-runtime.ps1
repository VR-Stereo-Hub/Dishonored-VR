# Switch which OpenXR runtime the installed mod uses, without hand-editing the ini.
#
#   .\tools\vr-runtime.ps1            # show what is selected now
#   .\tools\vr-runtime.ps1 shim       # SteamVR through the bundled shim
#   .\tools\vr-runtime.ps1 vdxr       # Virtual Desktop's own runtime, pinned
#   .\tools\vr-runtime.ps1 auto       # follow whatever the system has registered
#
# WHY THIS EXISTS. [VR] XrRuntimeJson sets XR_RUNTIME_JSON, and the OpenXR loader
# reads that BEFORE the system default - so a path left in the ini silently wins
# over whatever you picked in SteamVR or Virtual Desktop, every launch. That cost
# a session: SteamVR was selected and the mod kept coming up on VDXR because the
# ini still named it.
#
# The three modes are not interchangeable, and `auto` does NOT mean "the shim":
#   auto   try the system's native 32-bit runtime, fall back to the shim only if
#          there is none. With SteamVR selected this gives NATIVE SteamVR OpenXR,
#          which is the path that renders upside down (VR-146).
#   shim   dvr_steamvr32.dll directly, OpenXR-on-OpenVR. SteamVR must be running.
#   vdxr   pin Virtual Desktop's runtime by path, whatever else is registered.
#
# Writes through WritePrivateProfileString so the file's CRLF survives, and
# verifies the line endings afterwards (docs/TRAPS.md: the CRLF editing trap).
param([ValidateSet('shim','vdxr','auto','show')][string]$Mode = 'show')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\game-path.ps1')
$ini = Get-DvrIniPath
if (-not (Test-Path $ini)) { Write-Output "no ini at $ini - run the game once first"; exit 1 }

Add-Type -Namespace W -Name I -MemberDefinition '[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet=System.Runtime.InteropServices.CharSet.Ansi)] public static extern bool WritePrivateProfileString(string s, string k, string v, string f);'

function Show-Current {
    $r = (Get-Content $ini | Select-String '^Runtime=').ToString()
    $j = (Get-Content $ini | Select-String '^XrRuntimeJson=').ToString()
    Write-Output "  $r"
    Write-Output "  $j"
    if ($j -match '^XrRuntimeJson=\s*$') {
        Write-Output '  -> no path pinned; the system default decides.'
    } else {
        Write-Output '  -> PINNED. This overrides whatever you select in SteamVR or Virtual Desktop.'
    }
}

if ($Mode -eq 'show') { Write-Output 'Current runtime selection:'; Show-Current; exit 0 }

$vdxr = 'C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr-32.json'
switch ($Mode) {
    'shim' { $runtime = 'steamvr'; $json = '' }
    'vdxr' { $runtime = 'native';  $json = $vdxr }
    'auto' { $runtime = 'auto';    $json = '' }
}

$before = [IO.File]::ReadAllBytes($ini).Length
[W.I]::WritePrivateProfileString('VR','Runtime',$runtime,$ini) | Out-Null
[W.I]::WritePrivateProfileString('VR','XrRuntimeJson',$json,$ini) | Out-Null

$b = [IO.File]::ReadAllBytes($ini)
$crlf = 0; for ($i = 0; $i -lt $b.Length - 1; $i++) { if ($b[$i] -eq 13 -and $b[$i+1] -eq 10) { $crlf++ } }
$lf = ($b | Where-Object { $_ -eq 10 }).Count
if ($crlf -ne $lf) { Write-Output "WARNING: line endings are mixed now (CRLF $crlf, LF $lf) - check the ini"; }

Write-Output "runtime set to '$Mode':"
Show-Current
if ($Mode -eq 'shim') { Write-Output '  Start SteamVR BEFORE launching the game; the shim talks to it through OpenVR.' }
if ($Mode -eq 'auto') { Write-Output '  With SteamVR selected this gives NATIVE SteamVR OpenXR, not the shim (VR-146: upside down).' }
Write-Output "  ini $ini ($before -> $($b.Length) bytes)"
exit 0
