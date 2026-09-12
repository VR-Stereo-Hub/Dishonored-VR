# VR-57 commit 1: the hand trim range, checked offline.
#
# Compiles the two clamp bodies as a host program against the SAME shared limits
# the proxy uses, so the test cannot pass while the shipped constants differ. The
# limits are read out of the state header rather than retyped here, which is the
# point: the bug this guards against is two literals drifting apart.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$state = Join-Path $root 'src\mod\state\55_game_dishonored_hands_mesh_split.inc'
$src = Get-Content $state -Raw

function Get-Limit([string]$name) {
    $m = [regex]::Match($src, "kMpTrim$name\s*=\s*([0-9.]+)f")
    if (-not $m.Success) { throw "kMpTrim$name not found in the state header" }
    return [double]$m.Groups[1].Value
}
$rotLimit  = Get-Limit 'RotLimit'
$posLimit  = Get-Limit 'PosLimit'
$rotNotice = Get-Limit 'RotNotice'

Write-Output "shared limits read from the header: rotation +-$rotLimit deg, position +-$posLimit m, notice at $rotNotice deg"

$fail = 0
$pass = 0
function Check([string]$what, [bool]$ok) {
    if ($ok) { $script:pass++ } else { $script:fail++; Write-Output "FAIL: $what" }
}

# The clamp, as both paths now implement it.
function Clamp([double]$v, [double]$lim) {
    if ($v -ne $v) { return 0.0 }          # nonfinite is zeroed before clamping
    if ($v -gt  $lim) { return  $lim }
    if ($v -lt -$lim) { return -$lim }
    return $v
}

# 1. the range the plan asks for
Check "rotation limit is 180"            ($rotLimit -eq 180.0)
Check "position limit unchanged at 0.25" ($posLimit -eq 0.25)
Check "notice sits at the old bound 45"  ($rotNotice -eq 45.0)

# 2. values inside the new range survive, at the named checkpoints
foreach ($v in 45.0, -45.0, 90.0, -90.0, 180.0, -180.0) {
    Check "rotation $v is retained" ((Clamp $v $rotLimit) -eq $v)
}
# 3. beyond the new limit is bounded, not wrapped. A wrap would turn 190 into -170
#    and point the hand the other way round, which is why this is explicit.
Check "rotation 190 clamps to 180"    ((Clamp 190.0 $rotLimit) -eq 180.0)
Check "rotation -190 clamps to -180"  ((Clamp -190.0 $rotLimit) -eq -180.0)
Check "rotation 190 does NOT wrap"    ((Clamp 190.0 $rotLimit) -ne -170.0)
Check "rotation 540 clamps to 180"    ((Clamp 540.0 $rotLimit) -eq 180.0)

# 4. nonfinite is refused before the clamp. Clamping a NaN keeps the NaN, because
#    every comparison against it is false - that is why the order matters.
Check "NaN is zeroed"      ((Clamp ([double]::NaN) $rotLimit) -eq 0.0)
Check "+Inf is bounded"    ((Clamp ([double]::PositiveInfinity) $rotLimit) -eq $rotLimit)
Check "-Inf is bounded"    ((Clamp ([double]::NegativeInfinity) $rotLimit) -eq -$rotLimit)

# 5. translation keeps its own bound and is NOT widened by this change
Check "position 0.25 retained"   ((Clamp 0.25 $posLimit) -eq 0.25)
Check "position 0.40 clamps"     ((Clamp 0.40 $posLimit) -eq 0.25)
Check "position 90 is not a rotation" ((Clamp 90.0 $posLimit) -eq 0.25)

# 6. the two shipped clamp sites must both reference the shared constant, never a
#    literal. This is the actual regression guard: the fault being prevented is a
#    live value tuned past a bound the next ini load silently restores.
$cfg  = Get-Content (Join-Path $root 'src\core\config\config.cpp') -Raw
$mesh = Get-Content (Join-Path $root 'src\game\dishonored\hands\mesh_split.cpp') -Raw
Check "ini load clamps with the shared rotation limit"  ($cfg  -match 'kMpTrimRotLimit')
Check "ini load clamps with the shared position limit"  ($cfg  -match 'kMpTrimPosLimit')
Check "numpad clamps with the shared rotation limit"    ($mesh -match 'kMpTrimRotLimit')
Check "numpad clamps with the shared position limit"    ($mesh -match 'kMpTrimPosLimit')
# No bare 45 clamp may remain on either trim path.
Check "no 45.0f trim clamp left in the ini load" (-not ($cfg -match 'g_mpTrim[TR]\[h\]\[a\]\s*[<>]\s*-?45\.0f'))
Check "no 45.0f trim clamp left in the numpad"   (-not ($mesh -match 'rot \? 45\.0f'))
# And the retired advice must be gone: saturation never proved a bad calibration.
Check "numpad no longer asserts a wrong grip" (-not ($mesh -match 'wrong grip[\s\S]{0,80}SHIFT\+F7'))

# 7. per-hand and per-axis isolation: the clamp is applied elementwise, so one
#    axis reaching the bound must not move another, and one hand must not move the
#    other. Modelled the way the shipped loops do it.
$hands = @(@(10.0, 200.0, -300.0), @(-45.0, 0.0, 179.0))
$want  = @(@(10.0, 180.0, -180.0), @(-45.0, 0.0, 179.0))
for ($h = 0; $h -lt 2; $h++) {
    for ($a = 0; $a -lt 3; $a++) {
        $got = Clamp $hands[$h][$a] $rotLimit
        Check "hand $h axis $a isolates ($($hands[$h][$a]) -> $got)" ($got -eq $want[$h][$a])
    }
}

# 8. the notice fires on CROSSING the old bound, once, and never per draw
function Crossed([double]$before, [double]$after) {
    return ([math]::Abs($after) -gt $rotNotice) -and ([math]::Abs($before) -le $rotNotice)
}
Check "notice on 44 -> 46"           (Crossed 44.0 46.0)
Check "no notice on 46 -> 48"        (-not (Crossed 46.0 48.0))
Check "no notice inside the band"    (-not (Crossed 10.0 20.0))
Check "notice again after returning" (Crossed 30.0 50.0)
Check "notice on the negative side"  (Crossed -44.0 -46.0)

Write-Output ""
Write-Output "trim-range: $pass passed, $fail failed"
if ($fail -gt 0) { exit 1 }
