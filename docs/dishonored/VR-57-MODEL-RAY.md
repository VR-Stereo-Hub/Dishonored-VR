# The model ray: aiming from the weapon, not the controller (VR-57)

2026-09-12. What ships, how it was derived, and every fault that was found building
it. Supersedes the two working drafts from the same day.

The guide and the crossbow's shot now sit on the weapon's barrel line rather than on
the bare controller, the ray follows the hand trim, and the measurement survives a
relaunch. Confirmed in a headset.

---

## 1. What it is

A single ray, published once and consumed by everything:

```
the loaded bolt's own lengthwise axis, measured in the palm frame
        |
        +--> the dot and beam (compositor quads)
        +--> fire_frame() -> the native pre-spawn hook -> the bolt's direction
```

Because the guide and the shot consume **one** published ray, they cannot disagree.
That is structural, not maintained by hand, and it is asserted by
`tools/bolt-axis-host.ps1`.

## 2. Why the bolt, and not the weapon

The loaded bolt is the only unambiguous geometry in the view model. Measured: **335:1
axial variance, 44.5 units long**. A fit that demands 16:1 accepts it trivially.

A weapon body cannot supply the axis, and this was tried and removed rather than
assumed:

* **The crossbow's widest dimension is its bow arms, ACROSS the barrel.** A fitted long
  axis would aim the shot sideways with full confidence.
* **The geometry reader accepts at most 1024 vertices.** The weapon meshes measure
  **1961** (crossbow), **2481** (sword) and up to **6330** elsewhere, so every body was
  rejected by the size check before any axis was fitted - and the refusal message
  quoted the variance test it had never reached, which is its own small lesson.
* They are skinned to more than one bone, which the single-slot rigid transform cannot
  carry.

Making a body work needs vertex subsampling and multi-bone handling. It is not done.

And "line the ray up with the top of the model" is an offset chosen by eye, which is
the guessed constant this project's rules forbid shipping as a measured one - the same
objection that retired the per-weapon rotation the same day.

## 3. One axis for every weapon, latched

Per-weapon measurement was built, broke repeatedly, and was replaced. The faults are
in section 6; the design that replaced them is simply:

* **Measure strictly.** Only the regular bolt, and only while the crossbow is the
  equipped weapon, because the forward sign is resolved against the equipped weapon's
  forward.
* **Use permissively.** Once measured it is the shared ray for every weapon and every
  ammunition - crossbow, all three bolt types, the pistol, grenades, the rewire tool,
  the heart, the powers.
* **Never discard it.** Not on a weapon switch, an ammunition change, or a reload. An
  axis that cannot be discarded cannot be inverted by a switch, which removes the
  class of fault rather than an instance.

Every ranged weapon here is held the same way, so one measured forward serves all of
them; what the ray has to follow is the hand, not a silhouette.

## 4. It follows the hand, and it survives a relaunch

**Hand trim.** The ray is stored in the PALM frame, and that frame is rebuilt from the
current trim every time the ray is used. So tuning the hand with the numpad carries the
dot, the beam and the shot with it, and no re-measurement is needed. `FollowHandTrim`
does the same transport for the controller ray when `ModelRay` is off; the derivation
is in `hand_frame.h` (`follow_trim_ray`), checked against an independent reference.

**Across launches.** The axis can only be measured from a drawn crossbow bolt, so a
session that loads a save with the pistol out never measures one and had no guide at
all. It is now written to the ini on first measurement and restored at startup. It
ships as a default paired with the grip.

**The grip is the pairing, not the trim.** The grip defines the palm frame the ray is
expressed in, so a stored ray is valid only against the grip it was measured with; a
mismatch beyond half a degree discards the record and names both values. Trim changes
need no record, for the reason above. A restored record passes the same bounds as a
live measurement, so an edited entry cannot install a ray a measurement would refuse.

## 5. Levers

| Key | Default | What |
|---|---|---|
| `[Aim] ModelRay` | **1** | the measured axis; overrides FollowHandTrim when on |
| `[Aim] FollowHandTrim` | **1** | carry the hand trim onto the controller ray (used when ModelRay is off) |
| `[Aim] FireFromHand` | **1** | the native pre-spawn hook aims the bolt at the published endpoint |
| `[Crosshair] Dot` / `Hand` | **1** / left | the guide itself |
| `[Hands] ModelAxisL*` | the measured set | the axis, its direction, and the grip it belongs to |
| `[Aim] ShotProbe` / `FireWatch` / `SeamProbe` / `DriveFromHand` | 0 | diagnostics and the retired cache drive |

The first four ship ON, which is a deliberate exception to default-OFF: the feature is
headset-confirmed and is the branch's purpose, the A/B is still one key each, and the
shipped `ModelAxisL*` means a first launch has a correct guide rather than none.

## 6. The faults, each found and fixed

Kept because every one of them cost a headset run, and most were mine.

| Symptom | Cause |
|---|---|
| Bolt never followed the guide | `FireFrame` was published BEFORE the transport changed the ray, while the visuals used the changed one. One ray was claimed; two were handed out. |
| No laser at all, 0 dispatches | The probe call sat inside the MotionAim arming window, which is only set while MotionAim is ENABLED - and it ships disabled. |
| The aim point was 348 m away | `c5` carries the camera position NEGATED. Launch (15195 8100 2868) against origin (-15053 -8230 -2854), an exact mirror through the world origin. Now 0.5-0.8 m. |
| Pistol and poison bolt had no laser | The candidate test was a single literal asset name, `bolt_01`. |
| The dot moved when ammunition changed | `bolt_01` is not one mesh: measured at length 44.531 AND 20.708. Different shapes, one name, different tips. |
| Crossbow mirrored after a pistol switch | A bolt is drawn while the pistol is equipped, and nothing checked the projectile belonged to the equipped weapon: `'bolt_01' axis adopted for weapon 'EliteGun'`, its sign resolved against the pistol's forward. |
| The axis was discarded every frame | The weapon name arrives EMPTY on the projectile's own draw, so an empty stored name compared as a mismatch - 408 adoptions in one run, all under weapon `'?'`. |
| Correct, then wrong after a reload | The measurement could be taken while the bolt was being ANIMATED. One frame mid-reload became the session's ray. |
| A wrong axis latched at startup | The origin bound allowed 2 m PER AXIS, 3.4 m from the palm, for a tip that sits 0.41 m away. And five frames can pass in 50 ms. |
| Guide vanished until the next shot | A latched axis is a CONSTANT, not a sample. It was published with a timestamp and expired whenever no weapon draw reached the measurement code. |
| A laser in the wrong place that later jumped | While the axis was pending, the controller ray was shown - and it carries the baseline offset the measured axis exists to remove. Nothing is shown now until the axis latches. |

## 7. What is deliberately not here

* **Pistol SHOTS.** The native fire hook is installed for the player's crossbow firing
  context only (`fireaim: INSTALLED ... player crossbow firing context only`), so
  pistol shots follow the engine's own aim no matter where the guide points. Its fire
  path needs tracing and hooking on its own address. **This is the next piece of work.**
* **The pistol's own measured axis.** Its loaded bullet is reported by the attach with
  an all-zero component transform, so it can never be a verified instance; its body
  mesh is past the reader's limits. It uses the shared bolt axis instead.
* **The aim assist's pull** toward the game's own solution. Switches located in
  `VR-57-AIM-PIPELINE.md`; untouched.
* **A weapon-body axis**, for the reasons in section 2.
* **Ballistic impact.** The dot is a launch target; gravity and the assist still apply,
  and a launch-line intersection is not an observed hit.

## 8. Verification

`tools/bolt-axis-host.ps1` - 660 checks: the axis fit, reflected bases and grip
parity, the palm transport against an independently built reference, native
muzzle-to-endpoint convergence, the candidate selection (weapon bodies must stay
refused), and the projectile/weapon pairing including the row that actually failed.
It also asserts the publication contract: everything that can change the ray runs
before the single fire publication, the visual is built after it, a pending axis
suppresses the guide, and the controller fallback stays gated on `ModelRay` being off.

`tools/follow-trim-host.ps1` - 62 checks on the trim transport. `trim-range` 37,
`fire-aim` 2,851, `aim-ray` 70,234.
