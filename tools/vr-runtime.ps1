# Switch which OpenXR runtime the mod uses, without hand-editing the ini.
#
#   vr-runtime.ps1            # show what is selected now
#   vr-runtime.ps1 shim       # SteamVR through the bundled shim (ships as the default)
#   vr-runtime.ps1 vdxr       # Virtual Desktop's own runtime, pinned by path
#   vr-runtime.ps1 auto       # follow whatever the system has registered
#
# WHY THIS EXISTS. [VR] XrRuntimeJson sets XR_RUNTIME_JSON, and the OpenXR loader
# reads that BEFORE the system default - so a path left in the ini silently wins
# over whatever you picked in SteamVR or Virtual Desktop, every launch. That cost
# a session: SteamVR was selected and the mod kept coming up on VDXR because the
# ini still named it, and the log said so the whole time.
#
# The three modes are NOT interchangeable, and `auto` does not mean "the shim":
#   shim   dvr_steamvr32.dll directly, OpenXR-on-OpenVR. SteamVR must already be
#          running. This is what the shipped ini selects.
#   vdxr   pin Virtual Desktop's runtime by path, whatever else is registered.
#   auto   try the system's native 32-bit runtime, fall back to the shim only if
#          there is none. With SteamVR selected that gives NATIVE SteamVR OpenXR,
#          which is the path recorded as rendering upside down (VR-146).
#
# Writes through WritePrivateProfileString so the file's CRLF survives, and
# checks the line endings afterwards (the ini is CRLF and hand-editing breaks it).
param(
    [ValidateSet('shim','vdxr','auto','show')][string]$Mode = 'show',
    [string]$GameDir = '',
    [string]$Json = ''
)
$ErrorActionPreference = 'Stop'

# Two homes, one script. In the repo a shared helper knows where Steam put the
# game. In the release zip there is no lib/ folder, and the convention the other
# shipped scripts use is that they sit in the game directory themselves
# (collect-support.ps1 defaults its GameDir to $PSScriptRoot). -GameDir wins.
$ini = ''
if ($GameDir) {
    $ini = Join-Path $GameDir 'dishonored_vr.ini'
} else {
    $lib = Join-Path $PSScriptRoot 'lib\game-path.ps1'
    if (Test-Path $lib) { . $lib; $ini = Get-DvrIniPath }
    else { $ini = Join-Path $PSScriptRoot 'dishonored_vr.ini' }
}
if (-not (Test-Path $ini)) {
    Write-Output "no dishonored_vr.ini at: $ini"
    Write-Output 'Put this script in the game folder beside the ini, or pass:'
    Write-Output '    -GameDir "<Steam>\steamapps\common\Dishonored\Binaries\Win32"'
    exit 1
}

Add-Type -Namespace W -Name I -MemberDefinition '[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet=System.Runtime.InteropServices.CharSet.Ansi)] public static extern bool WritePrivateProfileString(string s, string k, string v, string f);'

function Show-Current {
    $r = (Get-Content $ini | Select-String '^Runtime=' | Select-Object -First 1)
    $j = (Get-Content $ini | Select-String '^XrRuntimeJson=' | Select-Object -First 1)
    Write-Output "  $r"
    Write-Output "  $j"
    if ("$j" -match '^XrRuntimeJson=\s*$') {
        Write-Output '  -> no path pinned; Runtime above decides.'
    } else {
        Write-Output '  -> PINNED. This overrides whatever you select in SteamVR or Virtual Desktop.'
    }
}

if ($Mode -eq 'show') { Write-Output 'Current runtime selection:'; Show-Current; exit 0 }

$vdxrDefault = 'C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr-32.json'
$vdxr = if ($Json) { $Json } else { $vdxrDefault }
switch ($Mode) {
    'shim' { $runtime = 'steamvr'; $json = '' }
    'vdxr' { $runtime = 'native';  $json = $vdxr }
    'auto' { $runtime = 'auto';    $json = '' }
}
if ($Mode -eq 'vdxr' -and -not (Test-Path $vdxr)) {
    Write-Output "REFUSED: no runtime manifest at $vdxr"
    Write-Output 'Virtual Desktop may be installed elsewhere. Either pass the real path:'
    Write-Output '    -Json "<...>\virtualdesktop-openxr-32.json"'
    Write-Output 'or use `auto`, which lets the system decide instead of pinning anything.'
    exit 1
}

$before = [IO.File]::ReadAllBytes($ini).Length
[W.I]::WritePrivateProfileString('VR','Runtime',$runtime,$ini) | Out-Null
[W.I]::WritePrivateProfileString('VR','XrRuntimeJson',$json,$ini) | Out-Null

$b = [IO.File]::ReadAllBytes($ini)
$crlf = 0
for ($i = 0; $i -lt $b.Length - 1; $i++) { if ($b[$i] -eq 13 -and $b[$i+1] -eq 10) { $crlf++ } }
$lf = ($b | Where-Object { $_ -eq 10 }).Count
if ($crlf -ne $lf) { Write-Output "WARNING: mixed line endings now (CRLF $crlf, LF $lf) - check the ini" }

Write-Output "runtime set to '$Mode':"
Show-Current
if ($Mode -eq 'shim') { Write-Output '  Start SteamVR BEFORE launching; the shim talks to it through OpenVR.' }
if ($Mode -eq 'auto') { Write-Output '  With SteamVR selected this is NATIVE SteamVR OpenXR, not the shim (VR-146: upside down).' }
Write-Output "  $ini ($before -> $($b.Length) bytes)"
exit 0
