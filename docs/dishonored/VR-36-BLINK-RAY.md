# Blink aimed by the controller ray (VR-36)

2026-09-13. The plan, what the code already had, and what one build changes.

---

## 1. What was already there, and why it was not the answer

`blink.cpp` has carried hand aiming since build 32.x, behind `[Blink] ControllerAim`
(installed 0). Four native seams were derived and are documented in ENGINE_NOTES:

| Seam | Address | What it is |
|---|---|---|
| `BlinkDirHook` | `0xbf55a3` | the aim VECTOR at its source, before the engine traces. `[Blink] AimAtSource=1` |
| `BlinkAimHook` | `0xbf595f` | the direction locals inside the range multiply |
| `BlinkTraceHook` | `0xbf5d1a` | the trace END point |
| `BlinkDestHook` | `0xbf5e4f` | the destination, AFTER the engine traced and validated |

All four called one direction source: `BlinkControllerDir`, which builds the ray from
the raw device GRIP pose plus a 40 degree tilt (`[MotionAim] PitchOffsetDeg`) and
re-projects it through the head snapshot. **That is the legacy MotionAim ray**, the
one VR-57 replaced - the pistol and crossbow moved off it onto the published ray, and
its baseline offset is the "up and behind" error that cost three sessions.

So Blink was not one ray with the rest of the mod. It was the only consumer left on
the old one, and with `ControllerAim=0` it was not aiming by hand at all.

## 2. What the log already proves

From the 2026-09-13 run, with `ControllerAim=0`:

```
blinkdir: 85 calls (85 ours) | engine dir (-1042.62,227.42,266.87) len 1100 | driving=0
blinkdir: 180 calls (180 ours) | engine dir (-542.83,839.31,459.22) len 1100 | driving=0
blinkdir: 77 calls (77 ours) | engine dir (1097.07,-75.43,27.42) len 1100 | driving=0
```

Two facts, free:

* **The source seam is live and it is ours.** Every call matched the player's
  `ActivePowerComponent_Blink`; nothing else needs installing.
* **The length is constant at 1100 uu.** That retires the 32.69 claim that the
  engine's magnitude is already shortened by the view (it was read as varying:
  1434, 1600, 1001, 1600). On this build it is the power's reach, full stop. So
  handing the engine `our direction * its own length` keeps the reach exactly and
  lets the engine's trace do the shortening, the collision and the refusal.

## 3. The design

One function, `BlinkAimRayWorld`, is the only direction source in the module:

```
dvr::aim::fire_frame()            the published ray - the same one the dot,
        |                         the laser and the crossbow/pistol shots use
        v
dvr::fireaim::solve(...)          VR-57/VR-82's converter: XR LOCAL metres ->
        |                         game world units, through the head basis and
        |                         the view yaw/pitch, at [PosTrack] Scale
        v
  origin (the controller, in world uu) and a unit direction
```

Nothing new is derived. `solve()` is the crossbow's own converter and carries its
freshness, finiteness and reach guards; a refusal is a refusal here too.

**Injection stays at the source.** `BlinkDirHook` swaps the pointer the engine reads
its aim vector from, three instructions before the length is accumulated. Everything
downstream - the range multiply, the trace, the collision pull-back, the decal, the
destination - is then the engine's own work along our ray. The marker and the landing
point cannot disagree, because there is only one trace and the engine performed it.

**The destination seam stops redirecting.** Writing `PowerBlink+0x60` happens AFTER
the engine traced and validated, so the point substituted there was never checked
against geometry - that is 32.51's teleport-through-walls, and it is exactly the
failure VR-36's pass criteria forbid. It stays installed, observe-only, as the
instrument that says whether the source redirect took.

**Fail soft is the engine's own head aim.** If the ray is stale, the calibration is
missing or `solve()` refuses, the hook returns without touching anything and Blink is
the game's, unchanged. There is no second-choice ray: the legacy path is available
only as a deliberate A/B (`[Blink] UseAimRay=0`), never as a silent fallback.

**Reach.** `BlinkReach` (hand pitch sets the distance, `ReachMode=2`) is unchanged and
now reads the pitch of the NEW ray. It shortens the vector we hand the engine, so the
engine traces the shortened vector and the marker follows it. Marker and landing stay
one point.

## 4. The open question this build answers

The trace start is a local we do not see at the source seam (`END = offset + [ebp-0x24]`;
the trace seam that would show it sits on a branch that never executes). If the start
is the camera and our ray starts at the hand, a parallel direction lands about the
hand-to-eye offset off the guide's line - roughly 0.3 m, about 1.7 degrees at the
1100 uu reach. That is small, and it is measurable rather than arguable:

* `blinkray:` logs, per activation burst, our direction against the engine's, the
  angle between them, and the gap between the controller and the camera in uu.
* `blinkdst:` now also logs the angle between the engine's own destination
  (`dest - camera`) and the vector it was handed. **Near zero means the trace starts
  at the camera**, and convergence is then worth adding; a large angle means it starts
  somewhere else and a convergence correction would be a guessed constant.

That number decides step two. It is not guessed in this build.

## 5. Levers

| Key | Default | Live |
|---|---|---|
| `[Blink] ControllerAim` | 0 (repo), armed in the test install | `blink on\|off`, F10 Blink |
| `[Blink] UseAimRay` | 1 | `blink ray aim\|legacy` - the A/B against the old MotionAim ray |
| `[Blink] AimAtSource` | 1 | ini; the source seam, which is the only one that redirects |
| `[Blink] ReachMode` / `NearUU` / `PitchNearDeg` / `PitchFarDeg` | 2 / 150 / -55 / -5 | F10 Blink |

`[Mode] GamepadOnly=1` still forces `ControllerAim` off, so view-aimed Blink is
unchanged there - that is a pass criterion and it is untouched code.

## 6. Dead levers found on the way

`[Blink] Marker` and `MarkerPullbackUU` have had no consumer since 41.0 removed the
DXVK fork that drew `dxvk_vr_mark`. Config reads them, the F10 panel offers them, and
nothing else mentions them. The panel now says so rather than offering a control that
cannot do anything.

## 7. What VR-90 can take from this

The conversion is the reusable part: `fire_frame()` plus `fireaim::solve()` gives any
engine consumer the published ray in world units, with the guards already written. The
Blink work adds the second half of the pattern - **inject at the input of the engine's
own computation, never at its output** - which is what lets the game keep its own
validation. The game's crosshair is an output; VR-90 will have to find the input that
feeds it, the way `0xbf55a3` was found here.
