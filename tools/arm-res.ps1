# arm-res.ps1 - arm a render resolution for the NEXT launch, with the game not
# running.
#
# WHY THIS EXISTS. The `res <W>x<H>` seam command is the normal route, but it
# only exists while the game is up, and the size it writes takes effect at the
# launch AFTER that - so arming a size from a cold machine used to cost two
# launches. Nothing in the mod writes the resolution at startup (ResRequest is
# reachable only from the seam command and the F10 Display panel), so with the
# game stopped the four files have to be written directly.
#
# This mirrors ResRequest / ResWriteGameIni / LaunchArgsWrite in
# src/core/window/render_size.cpp exactly, and the same four places:
#
#   1. <game>\dishonored_vr.ini            [Screen] RenderWidth/Height/Fullscreen/VirtualMode
#   2. <game>\dishonored_vr_launch.txt     -ResX= -ResY= -FullScreen -DvrVirtualMode
#   3. Documents\...\DishonoredEngine.ini  [SystemSettings] ResX/ResY/Fullscreen
#   4. Documents\...\DishonoredCompat.ini  ResX/ResY in every AppCompatBucket that has them
#
# (3) and (4) are belt and braces: the command line in (2) is the route the
# engine actually honours (UE3 parses it before any ini - ENGINE_NOTES, "The
# render size: the ini route is inert, the command line is honoured"), but if
# the IAT patch ever fails to install, the inis are the fallback, and leaving
# them naming a different size means that fallback silently renders at the OLD
# resolution. (4) is the AppCompat trap specifically: the bucket AppCompat
# picks at startup overwrites [SystemSettings], so a size written only to
# [SystemSettings] does not hold.
#
# Usage:
#   .\tools\arm-res.ps1 2750x2850            # fullscreen (the default)
#   .\tools\arm-res.ps1 2750x2850 -Windowed
#   .\tools\arm-res.ps1 -Status              # what is armed right now
#   .\tools\arm-res.ps1 -Clear               # hand the size back to the game
[CmdletBinding()]
param(
    [Parameter(Position = 0)][string]$Size = "",
    [switch]$Windowed,
    [switch]$NoVirtualMode,
    [switch]$Status,
    [switch]$Clear,
    [string]$GamePath = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "lib\game-path.ps1")

$game   = Get-DvrGamePath $GamePath
$cfgDir = Get-DvrConfigDir
$modIni = Join-Path $game "dishonored_vr.ini"
$launch = Join-Path $game "dishonored_vr_launch.txt"
$engine = Join-Path $cfgDir "DishonoredEngine.ini"
$compat = Join-Path $cfgDir "DishonoredCompat.ini"

# The same profile-string API the mod uses, so the files stay byte-compatible
# with what ResWriteGameIni produces (section order, spacing, CRLF).
$sig = @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet=System.Runtime.InteropServices.CharSet.Ansi, SetLastError=true)]
public static extern uint GetPrivateProfileString(string sec, string key, string def, System.Text.StringBuilder ret, uint size, string file);
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet=System.Runtime.InteropServices.CharSet.Ansi, SetLastError=true)]
public static extern bool WritePrivateProfileString(string sec, string key, string val, string file);
'@
if (-not ("DvrIni" -as [type])) { Add-Type -MemberDefinition $sig -Name DvrIni -Namespace "" | Out-Null }

function Get-Ini {
    param($sec, $key, $file, $def = "")
    $sb = New-Object System.Text.StringBuilder 512
    [void][DvrIni]::GetPrivateProfileString($sec, $key, $def, $sb, 512, $file)
    return $sb.ToString()
}

function Set-Ini {
    param($sec, $key, $val, $file)
    $old = Get-Ini $sec $key $file "?"
    if (-not [DvrIni]::WritePrivateProfileString($sec, $key, "$val", $file)) {
        throw "could not write [$sec] $key=$val to $file"
    }
    Write-Host ("    [{0}] {1}: {2} -> {3}" -f $sec, $key, $old, $val)
}

function Show-Armed {
    Write-Host "Armed right now:" -ForegroundColor Cyan
    Write-Host ("  {0}" -f $modIni)
    foreach ($k in @("RenderWidth", "RenderHeight", "RenderFullscreen", "VirtualMode")) {
        Write-Host ("    [Screen] {0} = {1}" -f $k, (Get-Ini "Screen" $k $modIni "(unset)"))
    }
    Write-Host ("  {0}" -f $launch)
    if (Test-Path $launch) { Write-Host ("    {0}" -f (Get-Content $launch -Raw).Trim()) }
    else { Write-Host "    (absent - the game picks its own size)" }
    Write-Host ("  {0}" -f $engine)
    foreach ($k in @("ResX", "ResY", "Fullscreen")) {
        Write-Host ("    [SystemSettings] {0} = {1}" -f $k, (Get-Ini "SystemSettings" $k $engine "(unset)"))
    }
    Write-Host ("  {0}" -f $compat)
    for ($b = 1; $b -le 4; $b++) {
        $sec = "AppCompatBucket$b"
        $x = Get-Ini $sec "ResX" $compat ""
        if ($x) { Write-Host ("    [{0}] {1}x{2}" -f $sec, $x, (Get-Ini $sec "ResY" $compat "")) }
    }
}

if ($Status) { Show-Armed; return }

if ($Clear) {
    Write-Host "Clearing the resolution ask (the game picks its own size):" -ForegroundColor Yellow
    Set-Ini "Screen" "RenderWidth"  0 $modIni
    Set-Ini "Screen" "RenderHeight" 0 $modIni
    if (Test-Path $launch) { Remove-Item $launch; Write-Host "    deleted $launch" }
    Write-Host "The game's own inis are NOT reverted (same as 'res 0x0') - setup-game-ini.ps1 -Restore does those."
    return
}

if ($Size -notmatch '^\s*(\d+)\s*[xX*]\s*(\d+)\s*$') {
    throw "Size must look like 2750x2850 (got '$Size'). -Status shows what is armed, -Clear hands it back."
}
$w = [int]$Matches[1]
$h = [int]$Matches[2]
# The same bound ResRequest enforces (render_size.cpp).
if ($w -lt 640 -or $h -lt 480 -or $w -gt 16384 -or $h -gt 16384) {
    throw "$($w)x$($h) refused (640x480..16384x16384), the same bound as the res command."
}
$full    = -not $Windowed
$virtual = -not $NoVirtualMode
$fullTxt = if ($full) { "fullscreen" } else { "windowed" }
$virtTxt = if ($virtual) { "ON" } else { "off" }
$mp      = $w * $h / 1e6

Write-Host ("Arming {0}x{1} {2} (VirtualMode {3}) for the next launch" -f $w, $h, $fullTxt, $virtTxt) -ForegroundColor Cyan
Write-Host ("  eye aspect {0:N4} - near-square keeps FovLever=0 filling the frustum (ENGINE_NOTES)" -f ($w / $h))
Write-Host ("  {0:N2} MP per eye; the measured fit is ~0.64 ms/MP on a ~5.6 ms fixed floor -> perf: tick about {1:N1} ms" -f $mp, (5.6 + 0.64 * $mp))

# 1. the mod's own ini
Write-Host "  $modIni"
if (-not (Test-Path $modIni)) { Write-Host "    (absent - it will be created; the mod writes the rest at first launch)" }
Set-Ini "Screen" "RenderWidth"      $w $modIni
Set-Ini "Screen" "RenderHeight"     $h $modIni
Set-Ini "Screen" "RenderFullscreen" $(if ($full) { 1 } else { 0 }) $modIni
Set-Ini "Screen" "VirtualMode"      $(if ($virtual) { 1 } else { 0 }) $modIni

# 2. the launch file - the route the engine honours
$mode = if ($full) { "-FullScreen" } else { "-Windowed" }
$vm   = if ($virtual) { " -DvrVirtualMode" } else { "" }
$line = "-ResX=$w -ResY=$h $mode$vm"
[System.IO.File]::WriteAllText($launch, $line, (New-Object System.Text.ASCIIEncoding))
Write-Host "  $launch"
Write-Host "    $line"

# 3 + 4. the game's own inis, backed up once each, exactly like ResWriteGameIni
if (-not (Test-Path $engine)) {
    Write-Warning "DishonoredEngine.ini not at $engine - run the game once so it exists. The command line above still carries the size."
} else {
    foreach ($f in @($engine, $compat)) {
        if ((Test-Path $f) -and -not (Test-Path "$f.pre-dvr")) {
            Copy-Item $f "$f.pre-dvr"
            Write-Host "  backed up $f -> .pre-dvr (once)"
        }
    }
    Write-Host "  $engine"
    Set-Ini "SystemSettings" "ResX"       $w $engine
    Set-Ini "SystemSettings" "ResY"       $h $engine
    Set-Ini "SystemSettings" "Fullscreen" $(if ($full) { "True" } else { "False" }) $engine

    if (-not (Test-Path $compat)) {
        Write-Warning "DishonoredCompat.ini not at $compat - if the game starts at the wrong size, AppCompat overwrote it from a bucket."
    } else {
        Write-Host "  $compat"
        $n = 0
        for ($b = 1; $b -le 4; $b++) {
            $sec = "AppCompatBucket$b"
            # Never invent a section: only buckets that already carry ResX are
            # touched, the same rule ResWriteGameIni follows.
            if (-not (Get-Ini $sec "ResX" $compat "")) { continue }
            Set-Ini $sec "ResX" $w $compat
            Set-Ini $sec "ResY" $h $compat
            $n++
        }
        Write-Host ("    {0} of 4 buckets set - the bucket AppCompat picks at startup overwrites [SystemSettings], so they all carry the size" -f $n)
    }
}

Write-Host ""
Write-Host "Armed. Takes effect at the NEXT launch." -ForegroundColor Green
Write-Host "The log should then show, in order:"
Write-Host "  res: launch: command line extended ... -ResX=$w -ResY=$h"
Write-Host "  res: handed the game our $($w)x$($h)@<hz> mode (slot ...)"
Write-Host "  res: CreateDevice - the game asked for $($w)x$($h)"
Write-Host "  capture: $($w)x$($h)"
Write-Host "  res: HONOURED"
Write-Host "  xr: swapchain pair $($w)x$($h)"
