# VR-198: run DishonoredVR-Launcher.exe end to end against a SCRATCH game folder and
# a scratch config folder, never the real ones, and assert what it wrote:
#   - the three DLLs match the build outputs byte for byte;
#   - dishonored_vr.ini differs from release\dishonored_vr.ini in exactly the
#     five keys the choices own (Runtime, XrRuntimeJson, RenderWidth,
#     RenderHeight, DataDir), keeps CRLF and has no BOM;
#   - the four game-ini lines changed and nothing else;
#   - a second run changes nothing but the record's date;
#   - a kept, hand-edited ini is left alone apart from those keys;
#   - Dishonored.exe running is refused;
#   - uninstall restores the backed-up d3d9.dll and keeps the ini.
#   .\tools\installer-smoke.ps1 [-Debug]
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([switch]$Debug)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$config = if ($Debug) { 'Debug' } else { 'RelWithDebInfo' }
$bin = Join-Path $repo "build\src\$config"
$versionText = Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
if ($versionText -notmatch 'project\(DishonoredVR VERSION ([0-9.]+)') { throw 'Cannot read launcher version' }
$version = $Matches[1]
$exe = Join-Path $bin "DishonoredVR-Launcher-v$version.exe"
if (-not (Test-Path $exe)) { throw "missing $exe - run tools\build.ps1 first" }
$scratchRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\')
$scratch = [IO.Path]::GetFullPath((Join-Path $scratchRoot 'dvr-installer-smoke'))
if ($scratch -ne "$scratchRoot\dvr-installer-smoke") { throw 'Invalid scratch path' }
if (Test-Path -LiteralPath $scratch) {
    $items = @(Get-Item -LiteralPath $scratch) + @(Get-ChildItem -LiteralPath $scratch -Force -Recurse)
    if ($items | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) { throw 'Scratch path contains a reparse point' }
    Remove-Item -LiteralPath $scratch -Recurse -Force
}
$game = Join-Path $scratch 'game\Binaries\Win32'
$cfg = Join-Path $scratch 'config'
New-Item -ItemType Directory -Force -Path $game, $cfg | Out-Null
# a stand-in Dishonored.exe that can be RUN, so "refuse while running" is real
Copy-Item "$env:SystemRoot\SysWOW64\ping.exe" (Join-Path $game 'Dishonored.exe')
# a foreign d3d9.dll (some other wrapper) to be backed up and restored
[IO.File]::WriteAllBytes((Join-Path $game 'd3d9.dll'), [byte[]](77, 90, 1, 2, 3))
# synthesised game inis in the real shape: CRLF, duplicate keys across sections
$engine = "[Engine.Engine]`r`nbSmoothFrameRate=TRUE`r`nMinSmoothedFrameRate=22`r`n`r`n[SystemSettings]`r`nDepthOfField=True`r`nFullscreen=True`r`nUseVsync=True`r`n`r`n[SystemSettingsMobile]`r`nDepthOfField=True`r`nUseVsync=True`r`n"
$input = "[Engine.PlayerInput]`r`nbEnableMouseSmoothing=TRUE`r`nMouseSensitivity=50`r`n`r`n[Other]`r`nbEnableMouseSmoothing=TRUE`r`n"
$ascii = [Text.Encoding]::ASCII
[IO.File]::WriteAllBytes((Join-Path $cfg 'DishonoredEngine.ini'), $ascii.GetBytes($engine))
[IO.File]::WriteAllBytes((Join-Path $cfg 'DishonoredInput.ini'), $ascii.GetBytes($input))

$fails = 0
function Assert($cond, $what) { if ($cond) { "  ok   $what" } else { "  FAIL $what"; $script:fails++ } }
function Run($argsList) {
    $p = Start-Process -FilePath $exe -ArgumentList $argsList -Wait -PassThru -NoNewWindow
    return $p.ExitCode
}
function Sha($p) { (Get-FileHash $p -Algorithm SHA256).Hash.ToLower() }
function IniDiff($a, $b) {
    $la = [IO.File]::ReadAllLines($a); $lb = [IO.File]::ReadAllLines($b)
    $n = [Math]::Max($la.Count, $lb.Count); $d = @()
    for ($i = 0; $i -lt $n; $i++) {
        $x = if ($i -lt $la.Count) { $la[$i] } else { $null }
        $y = if ($i -lt $lb.Count) { $lb[$i] } else { $null }
        if ($x -ne $y) { $d += "$x  ->  $y" }
    }
    return $d
}
function LineEndings($p) {
    $b = [IO.File]::ReadAllBytes($p); $crlf = 0; $lf = 0
    for ($i = 0; $i -lt $b.Length; $i++) { if ($b[$i] -eq 10) { $lf++; if ($i -gt 0 -and $b[$i-1] -eq 13) { $crlf++ } } }
    return @($crlf, $lf, ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF))
}
$common = @('--apply', '--game-dir', "`"$game`"", '--config-dir', "`"$cfg`"")

'1. refused while the game runs'
$ping = Start-Process -FilePath (Join-Path $game 'Dishonored.exe') -ArgumentList @('-t', '127.0.0.1') -PassThru -WindowStyle Hidden
Start-Sleep -Milliseconds 500
$rc = Run ($common + @('--op', 'install', '--runtime', 'steamvr'))
Assert ($rc -eq 2) "exit 2 while Dishonored.exe runs (got $rc)"
Assert ((Get-Item (Join-Path $game 'd3d9.dll')).Length -eq 5) 'the foreign d3d9.dll was not touched'
Stop-Process -Id $ping.Id -Force; Start-Sleep -Milliseconds 300

'2. fresh install: steamvr, balanced'
$rc = Run ($common + @('--op', 'install', '--runtime', 'steamvr', '--quality', 'balanced'))
Assert ($rc -eq 0) "exit 0 (got $rc)"
foreach ($n in @('d3d9.dll', 'dvr_steamvr32.dll')) { Assert ((Sha (Join-Path $game $n)) -eq (Sha (Join-Path $bin $n))) "$n matches the build output" }
Assert ((Sha (Join-Path $game 'openvr_api.dll')) -eq (Sha (Join-Path $repo 'third_party\openvr_headers\bin\win32\openvr_api.dll'))) 'openvr_api.dll matches the vendored one'
if (Test-Path (Join-Path $repo 'build/dlss_host/dvr_dlss_host64.exe')) {
    foreach ($helper in @('dvr_dlss_host64.exe','nvngx_dlss.dll','amd_fidelityfx_loader_dx12.dll','amd_fidelityfx_upscaler_dx12.dll')) {
        Assert ((Sha (Join-Path $game "dvr_dlss/$helper")) -eq (Sha (Join-Path $repo "build/dlss_host/$helper"))) "embedded $helper matches this build"
    }
}
Assert (Test-Path (Join-Path $game 'd3d9.dll.dvr-backup')) 'the foreign d3d9.dll was backed up'
$ini = Join-Path $game 'dishonored_vr.ini'
$diff = IniDiff (Join-Path $repo 'release\dishonored_vr.ini') $ini
"  ini diff: " + ($diff -join ' | ')
Assert ($diff.Count -eq 2) "exactly 2 lines differ from the shipped ini (Runtime, DataDir); got $($diff.Count)"
Assert (($diff -join "`n") -match 'Runtime=auto  ->  Runtime=steamvr') 'Runtime=steamvr'
Assert (($diff -join "`n") -match 'DataDir=D:\\dvr-data  ->  DataDir=$') 'DataDir emptied'
$le = LineEndings $ini
Assert ($le[0] -eq $le[1]) "CRLF count equals LF count ($($le[0]) / $($le[1]))"
Assert (-not $le[2]) 'no UTF-8 BOM'
$eng = [IO.File]::ReadAllText((Join-Path $cfg 'DishonoredEngine.ini'))
$inp = [IO.File]::ReadAllText((Join-Path $cfg 'DishonoredInput.ini'))
$expectEngine = $engine.Replace('bSmoothFrameRate=TRUE', 'bSmoothFrameRate=FALSE').Replace("[SystemSettings]`r`nDepthOfField=True`r`nFullscreen=True`r`nUseVsync=True", "[SystemSettings]`r`nDepthOfField=False`r`nFullscreen=True`r`nUseVsync=False")
$expectInput = $input.Replace("[Engine.PlayerInput]`r`nbEnableMouseSmoothing=TRUE", "[Engine.PlayerInput]`r`nbEnableMouseSmoothing=FALSE")
Assert ($eng -ceq $expectEngine) 'DishonoredEngine.ini: exactly the three lines changed, Mobile section untouched, CRLF kept'
Assert ($inp -ceq $expectInput) 'DishonoredInput.ini: exactly the one line changed, [Other] untouched'
Assert ((Get-ChildItem $cfg -Filter '*.dvr-backup').Count -eq 2) 'one timestamped backup per game ini'
$rec = Get-Content (Join-Path $game 'dishonored_vr_install.json') -Raw | ConvertFrom-Json
Assert ($rec.d3d9Sha256 -eq (Sha (Join-Path $bin 'd3d9.dll'))) 'the record carries the installed d3d9.dll sha256'
Assert ($rec.runtime -eq 'steamvr' -and $rec.renderWidth -eq 2750 -and $rec.renderHeight -eq 2850) 'the record carries the choices'

'3. second run: idempotent'
$before = Get-Content $ini -Raw; $engBefore = $eng
$rc = Run ($common + @('--op', 'install', '--runtime', 'steamvr', '--quality', 'balanced'))
Assert ($rc -eq 0) "exit 0 (got $rc)"
Assert ((Get-Content $ini -Raw) -ceq $before) 'dishonored_vr.ini unchanged'
Assert ([IO.File]::ReadAllText((Join-Path $cfg 'DishonoredEngine.ini')) -ceq $engBefore) 'DishonoredEngine.ini unchanged'
Assert ((Get-ChildItem $cfg -Filter '*.dvr-backup').Count -eq 2) 'no new backups when nothing changed'

'4. change settings: performance + vdxr with a manifest'
$vdxr = Join-Path $scratch 'vd\virtualdesktop-openxr-32.json'
New-Item -ItemType Directory -Force -Path (Split-Path $vdxr) | Out-Null
'{}' | Set-Content $vdxr -Encoding Ascii
$rc = Run ($common + @('--op', 'change', '--runtime', 'vdxr', '--quality', 'performance', '--vdxr-json', "`"$vdxr`""))
Assert ($rc -eq 0) "exit 0 (got $rc)"
$diff = IniDiff (Join-Path $repo 'release\dishonored_vr.ini') $ini
"  ini diff: " + ($diff -join ' | ')
Assert ($diff.Count -eq 5) "exactly 5 lines differ (Runtime, XrRuntimeJson, RenderWidth, RenderHeight, DataDir); got $($diff.Count)"
Assert (($diff -join "`n") -match 'RenderWidth=2750  ->  RenderWidth=2382') 'RenderWidth 2382'
Assert (($diff -join "`n") -match 'RenderHeight=2850  ->  RenderHeight=2468') 'RenderHeight 2468'
Assert (($diff -join "`n") -match 'Runtime=auto  ->  Runtime=native') 'Runtime=native for VDXR'
Assert (($diff -join "`n") -match [regex]::Escape("XrRuntimeJson=  ->  XrRuntimeJson=$vdxr")) 'XrRuntimeJson pinned'

'5. a hand-edited ini is kept: only the five keys move'
$text = Get-Content $ini -Raw
$text = $text.Replace('HeightOffsetM=0.060', 'HeightOffsetM=0.123').Replace('DataDir=', 'DataDir=E:\mine')
[IO.File]::WriteAllText($ini, $text, $ascii)
$rc = Run ($common + @('--op', 'install', '--runtime', 'auto', '--quality', 'quality'))
Assert ($rc -eq 0) "exit 0 (got $rc)"
$after = Get-Content $ini -Raw
Assert ($after.Contains('HeightOffsetM=0.123')) 'the F10 value survived'
Assert ($after.Contains('DataDir=E:\mine')) 'a DataDir set by hand is kept'
Assert ($after.Contains('RenderWidth=3012') -and $after.Contains('RenderHeight=3122')) 'Quality = 3012x3122'
Assert ($after.Contains("Runtime=auto`r`n")) 'Runtime=auto'

'5b. launcher preferences: the entire ini differs only in selected keys'
$expected = $after.Replace("DesktopMirrorOff=1", "DesktopMirrorOff=0").Replace("PhysicalCrouch=1", "PhysicalCrouch=0").Replace("[Rain]`r`nRecovery=1`r`nHide=0", "[Rain]`r`nRecovery=1`r`nHide=1").Replace("DpadModifier=1", "DpadModifier=2").Replace("DpadFlip=0", "DpadFlip=1").Replace("PauseChord=1", "PauseChord=0")
$rc = Run ($common + @('--op','change','--runtime','auto','--quality','quality','--mirror','on','--physical-crouch','off','--hide-rain-overlay','on','--dpad-modifier','2','--dpad-flip','on','--pause-chord','off'))
Assert ($rc -eq 0) 'preference apply succeeds'
$changed = [IO.File]::ReadAllText($ini)
Assert ($changed -ceq $expected) 'full ini equals expected: exactly six selected preference values change'
$le = LineEndings $ini
Assert ($le[0] -eq $le[1] -and -not $le[2]) 'preference apply keeps CRLF and no BOM'
$rc = Run ($common + @('--op','change','--runtime','auto','--quality','quality'))
Assert ($rc -eq 0 -and [IO.File]::ReadAllText($ini) -ceq $changed) 'omitted preference flags preserve all saved values'
$rc = Run ($common + @('--op','update','--keep-settings'))
Assert ($rc -eq 0 -and [IO.File]::ReadAllText($ini) -ceq $changed) 'DLL update preserves the complete tuned ini'
$rc = Run ($common + @('--op','change','--mirror','invalid'))
Assert ($rc -eq 1 -and [IO.File]::ReadAllText($ini) -ceq $changed) 'invalid boolean fails before writing'
$rc = Run ($common + @('--op','change','--dpad-modifier','3'))
Assert ($rc -eq 1 -and [IO.File]::ReadAllText($ini) -ceq $changed) 'retired modifier fails before writing'
$rc = Run ($common + @('--op','change','--runtime','steamvr','--quality','quality','--mirror','off'))
Assert ($rc -eq 0 -and [IO.File]::ReadAllText($ini).Contains("DesktopMirrorOff=1")) 'SteamVR saves the mirror-off choice without a runtime override'

$beforeTexture=[IO.File]::ReadAllText($ini)
$rc = Run ($common + @('--op','change','--runtime','steamvr','--quality','quality','--texture-memory','on'))
$afterTexture=[IO.File]::ReadAllText($ini)
Assert ($rc -eq 0 -and $afterTexture.Contains("Managed=paged`r`n") -and $afterTexture.Contains("ShadowSurfaces=1`r`n") -and $afterTexture.Contains("ShadowFullCopy=1`r`n")) 'texture compatibility enables all required keys'
Assert ($afterTexture -ceq $beforeTexture.Replace("`r`nManaged=shadow`r`n","`r`nManaged=paged`r`n").Replace("`r`nShadowSurfaces=0`r`n","`r`nShadowSurfaces=1`r`n")) 'texture selection changes only Managed and required ShadowSurfaces on this profile'
$rc = Run ($common + @('--op','change','--runtime','steamvr','--quality','quality'))
Assert ([IO.File]::ReadAllText($ini) -ceq $afterTexture) 'omitted texture flag preserves paged mode'
$rc = Run ($common + @('--op','change','--texture-memory','invalid'))
Assert ($rc -eq 1 -and [IO.File]::ReadAllText($ini) -ceq $afterTexture) 'invalid texture option refuses before writing'
$rc = Run ($common + @('--op','change','--runtime','steamvr','--quality','quality','--texture-memory','off'))
Assert ($rc -eq 0 -and [IO.File]::ReadAllText($ini) -ceq $beforeTexture.Replace("`r`nShadowSurfaces=0`r`n","`r`nShadowSurfaces=1`r`n")) 'texture off selects conventional shadows and retains the compatible surface redirect'
Assert ((Sha (Join-Path $game 'DishonoredVR_ReShade.addon32')) -eq (Sha (Join-Path $bin 'DishonoredVR_ReShade.addon32'))) 'launcher embeds this bridge build'

# Controller bindings use the same key/source names as F10 and keep untouched data.
'5b. controller mappings persist exactly the selected edits'
$bindBefore=[IO.File]::ReadAllText($ini)
$bindBefore += "`r`n[ControllerBinds]`r`nFutureBinding=KeepMe`r`nJump=A`r`nStealth=B`r`nSwapSticks=0`r`n"
[IO.File]::WriteAllText($ini,$bindBefore,$ascii)
$rc=Run ($common + @('--op','change','--runtime','steamvr','--quality','quality','--bind-Jump','B','--bind-Stealth','A','--bind-swap-sticks','on'))
$bindAfter=[IO.File]::ReadAllText($ini)
$bindExpected=$bindBefore.Replace("`r`nJump=A`r`n","`r`nJump=B`r`n").Replace("`r`nStealth=B`r`n","`r`nStealth=A`r`n").Replace("`r`nSwapSticks=0`r`n","`r`nSwapSticks=1`r`n")
Assert ($rc -eq 0 -and $bindAfter -ceq $bindExpected) 'whole INI changes only Jump, Stealth and SwapSticks'
Assert ($bindAfter.Contains('FutureBinding=KeepMe')) 'unknown binding key preserved'
$rc=Run ($common + @('--op','change','--runtime','steamvr','--quality','quality'))
Assert ($rc -eq 0 -and [IO.File]::ReadAllText($ini) -ceq $bindAfter) 'omitted binding flags preserve remaps'
$rc=Run ($common + @('--op','change','--bind-Jump','invalid'))
Assert ($rc -eq 1 -and [IO.File]::ReadAllText($ini) -ceq $bindAfter) 'invalid source refuses before writes'
$rc=Run ($common + @('--op','change','--bind-swap-sticks','invalid'))
Assert ($rc -eq 1 -and [IO.File]::ReadAllText($ini) -ceq $bindAfter) 'invalid stick option refuses before writes'
$le=LineEndings $ini
Assert ($le[0] -eq $le[1] -and -not $le[2]) 'binding edits preserve CRLF and encoding'


'5c. stereo selection preserves every unrelated setting'
$stereoBefore=[IO.File]::ReadAllText($ini)
$rc=Run ($common + @('--op','change','--runtime','steamvr','--quality','quality','--stereo','afw'))
$stereoAfter=[IO.File]::ReadAllText($ini)
Assert ($rc -eq 0 -and $stereoAfter -ceq $stereoBefore.Replace("`r`nMethod=reentry`r`n","`r`nMethod=afw`r`n")) 'AFW selection changes only Stereo Method in the whole INI'
$rc=Run ($common + @('--op','change','--runtime','steamvr','--quality','quality'))
Assert ($rc -eq 0 -and [IO.File]::ReadAllText($ini) -ceq $stereoAfter) 'omitted stereo flag preserves AFW'
$rc=Run ($common + @('--op','change','--stereo','invalid'))
Assert ($rc -eq 1 -and [IO.File]::ReadAllText($ini) -ceq $stereoAfter) 'invalid stereo flag fails before writing'
$rc=Run ($common + @('--op','change','--runtime','steamvr','--quality','quality','--stereo','stereo'))
Assert ($rc -eq 0 -and [IO.File]::ReadAllText($ini) -ceq $stereoBefore) 'Stereo restores the complete expected INI'

'5d. ReShade management keeps every unrelated byte and all presets'
$shadeRuntime=Join-Path $game 'ReShade32.dll'
$shadePreset=Join-Path $game 'UserPreset.ini'
[IO.File]::WriteAllBytes($shadeRuntime,[byte[]](77,90,12,34,56))
[IO.File]::WriteAllText($shadePreset,"[Techniques]`r`nUserSetting=kept`r`n",$ascii)
$shadeHash=Sha $shadeRuntime; $presetHash=Sha $shadePreset
$shadeBefore=[IO.File]::ReadAllText($ini)
$rc=Run ($common + @('--op','reshade-on'))
$shadeAfter=[IO.File]::ReadAllText($ini)
$shadeExpected=[regex]::Replace($shadeBefore,'(?ms)(^\[ReShade\]\r\n[^[]*?^Enabled=)0(?=\r?$)','${1}1')
Assert ($rc -eq 0 -and $shadeAfter -ceq $shadeExpected) 'ReShade enable changes only its Enabled value across whole INI'
$rc=Run ($common + @('--op','reshade-off'))
Assert ($rc -eq 0 -and [IO.File]::ReadAllText($ini) -ceq $shadeBefore) 'ReShade off restores entire expected INI'
$rc=Run ($common + @('--op','reshade-remove'))
Assert ($rc -eq 0 -and -not (Test-Path -LiteralPath $shadeRuntime)) 'runtime removed without executing it'
$shadeBackups=@(Get-ChildItem -LiteralPath $game -Filter 'ReShade32.dll.*.dvr-backup')
Assert ($shadeBackups.Count -eq 1 -and (Sha $shadeBackups[0].FullName) -eq $shadeHash) 'removed runtime retained as byte-exact backup'
Assert ((Sha $shadePreset) -eq $presetHash -and [IO.File]::ReadAllText($ini) -ceq $shadeBefore) 'presets and complete settings preserved on removal'
[IO.File]::WriteAllBytes($shadeRuntime,[byte[]](77,90,12,34,56))

'5d. running-game settings writes are refused'
$guardIni=Sha $ini
$guardGame=Sha (Join-Path $cfg 'DishonoredEngine.ini')
$ping = Start-Process -FilePath (Join-Path $game 'Dishonored.exe') -ArgumentList @('-t', '127.0.0.1') -PassThru -WindowStyle Hidden
try {
    Start-Sleep -Milliseconds 300
    $rc=Run ($common + @('--op','change','--stereo','afw'))
    Assert ($rc -eq 2 -and (Sha $ini) -eq $guardIni) 'settings change refuses while game runs and whole INI stays exact'
    $rc=Run ($common + @('--op','reshade-on'))
    Assert ($rc -eq 2 -and (Sha $ini) -eq $guardIni) 'ReShade refuses writes while game runs'
    $rc=Run ($common + @('--op','baseline'))
    Assert ($rc -eq 2 -and (Sha (Join-Path $cfg 'DishonoredEngine.ini')) -eq $guardGame) 'baseline refuses while game runs and game INI stays exact'
} finally { Stop-Process -Id $ping.Id -Force }

'5e. changing settings retains installed build identity'
$recordPath=Join-Path $game 'dishonored_vr_install.json'
$knownRecord=Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
$knownRecord.version='0.8.7'; $knownRecord.buildId='accepted-old-build'
[IO.File]::WriteAllText($recordPath,($knownRecord | ConvertTo-Json),$ascii)
$rc=Run ($common + @('--op','change','--runtime','steamvr','--quality','quality'))
$identity=Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
Assert ($rc -eq 0 -and $identity.version -eq '0.8.7' -and $identity.buildId -eq 'accepted-old-build') 'settings do not claim the embedded build was installed'
Assert ($identity.d3d9Sha256 -eq $knownRecord.d3d9Sha256) 'settings record keeps the actual installed DLL hash'

'6. disable / enable'
$rc = Run ($common + @('--op', 'disable')); Assert ($rc -eq 0 -and (Test-Path (Join-Path $game 'disable_vr.txt'))) 'disable_vr.txt written'
$rc = Run ($common + @('--op', 'enable')); Assert ($rc -eq 0 -and -not (Test-Path (Join-Path $game 'disable_vr.txt'))) 'disable_vr.txt removed'

'7. uninstall keeps the ini and restores the backup'
$rc = Run ($common + @('--op', 'uninstall'))
Assert ($rc -eq 0) "exit 0 (got $rc)"
Assert ((Get-Item (Join-Path $game 'd3d9.dll')).Length -eq 5) 'the foreign d3d9.dll is back'
Assert (-not (Test-Path (Join-Path $game 'd3d9.dll.dvr-backup'))) 'the backup was consumed'
foreach ($n in @('DishonoredVR_ReShade.addon32', 'dvr_steamvr32.dll', 'openvr_api.dll', 'dishonored_vr_install.json')) { Assert (-not (Test-Path (Join-Path $game $n))) "$n removed" }
Assert (Test-Path $ini) 'dishonored_vr.ini kept'
$rc = Run ($common + @('--op', 'uninstall', '--delete-ini'))
Assert (-not (Test-Path $ini)) 'dishonored_vr.ini deleted when asked'

'8. an outdated ini (version 13) is refreshed with a backup, and its runtime and size carry over'
$old = Get-Content (Join-Path $repo 'release\dishonored_vr.ini') -Raw
$old = $old.Replace("Version=15", "Version=13").Replace("RenderWidth=2750", "RenderWidth=2064").Replace("RenderHeight=2850", "RenderHeight=2208").Replace("Runtime=auto", "Runtime=steamvr").Replace("HeightOffsetM=0.060", "HeightOffsetM=0.111")
[IO.File]::WriteAllText($ini, $old, $ascii)
$rc = Run ($common + @('--op', 'update', '--keep-settings'))
Assert ($rc -eq 0) "exit 0 (got $rc)"
$after = Get-Content $ini -Raw
Assert ($after.Contains("Version=15`r`n")) 'refreshed to the embedded version'
Assert ($after.Contains("RenderWidth=2064`r`n") -and $after.Contains("RenderHeight=2208`r`n")) 'the old exact size carried over'
Assert ($after.Contains("Runtime=steamvr`r`n")) 'the old runtime carried over'
Assert ($after.Contains("DataDir=`r`n")) 'DataDir empty, not the dev drive'
Assert (-not $after.Contains('HeightOffsetM=0.111')) 'the old tuning is gone (as the mod refresh would have done)'
Assert (@(Get-ChildItem $game -Filter 'dishonored_vr.ini.*.dvr-backup' | Where-Object { [IO.File]::ReadAllText($_.FullName) -ceq $old }).Count -ge 1) 'the old ini is kept byte for byte beside it'
$rc = Run ($common + @('--op', 'uninstall', '--delete-ini'))
Remove-Item (Join-Path $game 'dishonored_vr.ini.*.dvr-backup') -Force

# The default reset must replace all tuning and preserve a byte-identical backup.
'8b. default update reset uses public defaults and backs up every setting'
[IO.File]::WriteAllText($ini, $changed, $ascii)
$rc = Run ($common + @('--op','update'))
Assert ($rc -eq 0) 'overwrite update succeeds'
$reset = [IO.File]::ReadAllText($ini)
$defaults = [IO.File]::ReadAllText((Join-Path $repo 'release\dishonored_vr.ini')).Replace('DataDir=D:\dvr-data','DataDir=')
Assert ($reset -ceq $defaults) 'entire reset ini matches shipped defaults with portable data path'
$resetRecord=Get-Content (Join-Path $game 'dishonored_vr_install.json') -Raw | ConvertFrom-Json
Assert ($resetRecord.runtime -eq 'auto' -and $resetRecord.quality -eq 'balanced') 'install record reflects reset choices'
$backups = @(Get-ChildItem -LiteralPath $game -Filter 'dishonored_vr.ini.*.dvr-backup')
Assert ($backups.Count -eq 1 -and [IO.File]::ReadAllText($backups[0].FullName) -ceq $changed) 'full original tuning has an exact backup'
$le = LineEndings $ini
Assert ($le[0] -eq $le[1] -and -not $le[2]) 'reset preserves CRLF without BOM'
'8c. failure after a DLL write restores the entire previous version'
$rollbackFiles=@('d3d9.dll','dvr_steamvr32.dll','openvr_api.dll','dishonored_vr.ini','dishonored_vr_install.json')
[IO.File]::WriteAllBytes((Join-Path $game 'd3d9.dll'),[byte[]](77,90,7,8,9))
$beforeHashes=@{}
foreach($name in $rollbackFiles) { $beforeHashes[$name]=Sha (Join-Path $game $name) }
$locked=[IO.File]::Open((Join-Path $game 'dvr_steamvr32.dll'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try { $rc=Run ($common + @('--op','update','--result',"`"$(Join-Path $scratch 'rollback.txt')`"")) } finally { $locked.Dispose() }
Assert ($rc -ne 0) 'locked second DLL causes update failure'
foreach($name in $rollbackFiles) { Assert ((Sha (Join-Path $game $name)) -eq $beforeHashes[$name]) "$name restored byte for byte after partial update" }
Assert ((Get-Content (Join-Path $scratch 'rollback.txt') -Raw).Contains('restored the previous version')) 'failure report confirms rollback'
$rc = Run ($common + @('--op','uninstall','--delete-ini'))

'9. install with no game config folder: baseline pending, not failed'
if ([IO.Path]::GetFullPath($cfg) -ne (Join-Path $scratch 'config')) { throw 'Invalid scratch config path' }
Remove-Item -LiteralPath $cfg -Recurse -Force
$rc = Run ($common + @('--op', 'install', '--runtime', 'auto'))
Assert ($rc -eq 0) "exit 0 with the config folder missing (got $rc)"

if ($fails) { throw "installer-smoke: $fails assertion(s) failed" }
"installer-smoke: all passed (scratch at $scratch)"
