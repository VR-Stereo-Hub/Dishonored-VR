# VR-33: the hands at the controllers

**Status: WORKING.** The hands track the controllers, are correctly scaled,
occlude against world geometry, and hold position through head turns. Confirmed
in the headset 2026-09-07.

This is the consolidated record for the branch
`claude/vr-33-hands-and-weapons-at-the-controllers`. It documents what the
mechanism is, what was measured to get there, and - at least as importantly -
the seven approaches that failed and why, because most of them looked right at
the time.

Recoverable tags: `vr33-placement-works`, `vr33-scale-correct`,
`vr33-hands-working`.

---

## 1. The mechanism

### The vertex path, read from the shaders' own disassembly

```
p_local = ( sum_i w_i * BoneMatrix[idx_i] ) * ( v0 * MeshExtension + MeshOrigin )
p_cam   = LocalToWorld * p_local            (camera-relative world, unreal units)
clip    = ViewProjectionMatrix * p_cam
```

The bone palette's output is the **component's local space**. `LocalToWorld`
takes it to a **camera-relative** world frame - world axes, origin at the
camera. `ViewProjectionMatrix` takes that to clip space.

### Placement

```
target_local = Rl^T * (d_cam - t)      [Rl | t] = LocalToWorld, read at the draw
T            = target_local - q_local  q re-skinned from the game's palette each frame
M_i_new      = M_i + T                 (translation only, on every bone of that hand)
```

`d_cam` is the controller expressed in the camera-relative frame, built from
the camera basis recovered from `ViewProjectionMatrix`:

* rows 0 and 1 normalised give **right** and **up**; their lengths are the
  focal scales,
* the **w row** is the **forward** axis, and is exactly unit for a standard
  perspective.

Finger animation survives because a common rigid transform commutes with the
skinning blend: `sum_i w_i (D M_i) v = D (sum_i w_i M_i v)`. The engine keeps
animating `M_i`; `T` only moves the result.

### The palm anchor

Placement needs to know where the visible palm *is* this frame. It skins a
fixed set of real vertices with the game's own unmodified palette - the
deduplicated vertices nearest the hand class's bind-pose centroid, so fingers
fall out as the extremities of that cloud with no need to identify a joint.

`q` always comes from the unmodified cache, never a palette already moved, or
the correction compounds frame on frame.

### The eye

The eye is decided **once per Present** from the right-axis jump in
`LocalToWorld`'s translation, and the **sign** of that jump gives left or right
absolutely: camera-relative positions are `world - camera`, so the right eye's
camera being further right makes its positions smaller on that axis.

Measured: successive original draws differ on that axis either by ~0 (the
several passes within one eye) or by **6.76 uu against a predicted IPD of
6.31 uu**. Head motion also moves the projection between Presents, so the band
is bounded on both sides and anything outside it leaves the eye **unknown**,
which applies no offset at all.

### The depth range

The view model is drawn with its viewport depth range crushed to
`MinZ 0.0, MaxZ 0.001` - the standard first-person guarantee that the weapon
and hands are never occluded. `[Hands] PaletteDepthRange` restores the full
range for our draws only and puts the game's own back on every exit path. That
is what gives correct occlusion.

---

## 2. Engine facts established

| Fact | Detail |
|---|---|
| Palette registers | `BoneMatrices` declared `float4x3[75]` at c6; 48 uploaded (144 registers). Bone `b` at `base + 3b`, `+1`, `+2` |
| Palette output space | The component's LOCAL space, not camera-relative and not world |
| `LocalToWorld` | c231; maps local to a CAMERA-RELATIVE world frame; rigid |
| `ViewProjectionMatrix` | c0; standard perspective, w row unit |
| Derived FOV | 108.1 x 110.0 degrees |
| Weight normalisation | The shader does **not** normalise. It sums `w_i M_i` into one blended matrix, so a translation `T` moves a vertex by `wsum * T`. Measured on the anchor: weights sum to exactly 1.0000 |
| Blend index order | The shader reads `a0` as `.yxzw`; the weight/index pairing is still correct, only the evaluation order differs |
| Shaders per mesh | THREE draw this mesh, and they disagree on layout: `MeshOrigin`/`MeshExtension` at c235/c236 in one and c238/c239 in the others; `WorldToLocal` at c235 in two |
| Shader immediates | One shader defines c4 as `(3, 1, 0, 0)` while the device reports `(0, 0, 0, 1)`. A shader immediate overrides the API, so a device read of that register is wrong by construction |
| Draws per frame | Each shader draws the mesh multiple times per frame; only one pass is depth-crushed |
| Eye separation | Present-to-present, in `LocalToWorld`'s translation, ~6.76 uu |
| SkelControls | Zero of 66 advance `ControlTickTag` over 34 samples with the hands subsystem live, across a full 103,117-object sweep. The native lane is measured shut, and parked - not proven impossible |

**Register numbers are never hard-coded.** Each shader's CTAB constant table is
parsed at runtime and a draw whose layout cannot be read is refused.

---

## 3. What failed, and why

Seven approaches. Most looked correct when they shipped.

### 3.1 The truncated GObjects census

The scan that closed the native SkelControl lane had a 64-entry comparison
table and a loop condition of `curN < 64`, so it **stopped when the table
filled**. "64 SkelControl objects" was the array's capacity read as a
population. Re-run as a full sweep it found 66 objects, none ticking - so the
conclusion survived, but it had not been evidence.

### 3.2 The relative drive

Moved each hand by its controller's travel from a captured neutral. Refuted on
paper before it was ever re-run: `p_out = h + R*b(t) + (w - w0)`. With the
controller and head still the correction is zero at every head *orientation*,
while the original camera-following hand `b(t)` keeps following the camera
underneath it. The cancellation argument covered the added displacement and
omitted the hand already there.

### 3.3 The head-space neutral

Storing the neutral in head-space coordinates made the zero point rotate with
the head, so a still controller produced a moving difference. It also made the
head yaw at capture a fixed rotation of the whole mapping.

### 3.4 The yaw residual

Assumed the camera carried a body yaw the head did not. The run measured
`phi` **constant at 144 deg across a 145 deg head swing**: the camera tracks
the head 1:1 and `phi` is the UE/XR yaw-origin offset, not a body yaw.
`g_bodyYawU` looked like the right source and turned out to be declared and
never written anywhere in the tree.

### 3.5 The calibrated origin

Absolute placement with `o = q(t0) - controller(t0)` folded the
coordinate-origin difference, any grip offset and any anchor error into one
138 uu constant, applied in a rotating frame - a 1.4 m lever pivoting on the
head. Its size was also misread as evidence for a pawn-root origin, using a
scale that was itself unvalidated.

### 3.6 The game-thread eye flag

`dvr::camera::second_pass_for_current_thread()` is the signal the camera seam
uses, but it is set on the **game thread** and UE3 queues render commands, so
it reads false by the time the draw executes on the render thread. The counter
caught it: `pass2 0 draws` against `pass1 92,800`.

### 3.7 The midpoint and ordinal eye classifiers

A learned midpoint of the right-axis projection is **not head-invariant** -
turning the head drifts the projection for reasons unrelated to the eye, so
draws crossed a stale midpoint and took a full IPD of error, which read as the
hands teleporting.

The ordinal replacement was sound in principle but **sampled per HAND**, so its
"pair" was the left and right hand of ONE draw, whose constants are identical by
construction. 24,376 zero comparisons that measured nothing - and which were
briefly reported as proof that `LocalToWorld` carries no eye information. It
does.

---

## 4. Instrument failures, and the rule they produced

Three instruments misled, and the pattern is the same each time: **they were
checked against expectation rather than against their own ability to fail.**

1. **A truncated sweep** reported its array capacity as a population.
2. **A midpoint classifier** reported a 6.8 uu spread that matched the IPD to
   8% and was measuring head motion.
3. **A per-hand sampler** compared a draw with itself and reported the zeroes
   as a finding.

And once, an **absence was reported that had never been established** - the
"hunt emits nothing" claim, when 17 lines were in the log and the grep pattern
was wrong.

**The rule: an instrument must declare and log the unit it sampled.** Draws
entered, sampled and rejected are separate counts; "nothing was sampled" must
never be able to read as "no difference was found"; and two hands of one draw
are ONE view sample, not a pair.

---

## 5. The levers

| Key | Default | What it does |
|---|---|---|
| `[Hands] Palette` | 0 | The draw-scoped bone palette backend. Required by everything below |
| `[Hands] PaletteWorld` | 0 | Placement through the measured chain. **This is the working mode** |
| `[Hands] PaletteEyeOffset` | 0 | Apply the per-eye offset. Without it the hands read as far too large |
| `[Hands] PaletteDepthRange` | 0 | Restore the full depth range so the hands occlude |
| `[Hands] PaletteDriveGain` | 1.0 | Scale trim on the metres-to-units conversion |
| `[Hands] PaletteWeightTol` | 0.02 | How far the anchor's weights may stray from summing to 1 before it refuses |
| `[Hands] PaletteStep` | 0 | The axis probe (F6 steps rest -> 0 -> 1 -> 2). Measured the basis |
| `[Hands] PaletteCapture` | 0 | Capture qualified draws for offline analysis (SHIFT+F6, or `pcap arm`) |
| `[Hands] PaletteEyeHunt` | 0 | The draw-to-draw comparison that found the eye |

A working headset configuration is `Palette=1`, `PaletteWorld=1`,
`PaletteEyeOffset=1`, `PaletteDepthRange=1`.

---

## 6. Known limits

* **Weapons do not follow.** This is a GPU-side edit to the skinning matrices;
  the engine's own transform is untouched, so the crossbow, bolt and muzzle
  effects stay where the engine put them.
* **No rotation.** The hands keep the engine's animated orientation, so they
  look posed rather than gripping.
* **Gameplay is unaffected.** Firing aim, projectile origin and melee contacts
  read the engine transform.
* **Scale is trimmed, not measured.** `[Hands] WorldScaleUU` is 100 while
  `[PosTrack] Scale` is 108. One canonical metres-to-units conversion has not
  been established.
* **Pose timing** - head look-ahead against hand prediction - is not addressed.
* **The eye tag** is derived, not carried. A render-view ticket from the stereo
  submission path would be more robust than inferring it from a jump.

---

## 7. Where the code is

| Piece | File |
|---|---|
| Placement, anchor, eye, depth lever | `src/game/dishonored/hands/mesh_split.cpp` |
| Globals and the recorded findings | `src/mod/state/55_game_dishonored_hands_mesh_split.inc` |
| The palette cache (register interval) | `src/core/framework/vs_const_hook.cpp` |
| Shader capture and CTAB reflection | `src/game/dishonored/hands/palette_capture.cpp` |
| Hotkeys (F6, SHIFT+F6) | `src/core/input/hotkeys.cpp` |
| Ini keys | `src/core/config/config.cpp` |
| Engine findings | `docs/dishonored/ENGINE_NOTES.md` |

The external reviews that shaped this work are
`VR-33-PALETTE-REVIEW.md`, `VR-33-BUILD-A-REVIEW.md`,
`VR-33-SHADER-CAPTURE-REVIEW.md` and `VR-33-FINAL-IMPLEMENTATION-HANDOFF.md`.
