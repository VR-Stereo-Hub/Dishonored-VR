# VR-33: why the weapons do not attach, and what to do instead

Status: **diagnosis complete, fix not yet implemented.** Written for review
before any code changes. Every number below is from the 2026-09-08 07:12 run
(`dishonored_vr.log`, build `2b2ee0d8`).

The reviewer's job: challenge the diagnosis in section 2, then pick between the
two routes in section 4. Section 3 lists defects that must be fixed whichever
route wins.

---

## 1. What happened

Three sweeps have now run end to end. All three were mechanically perfect: 16
phases, every hide and every restore verified by reading `HiddenMaterials`
back, and the tester watched the sword, the crossbow and the bolt each blink
twice on command. All three attributed **zero** draws to any component.

The current run:

```
wid: 428 distinct skinned draw signature(s) over 3259728 draw(s), 0 that did not fit
wid:   'Skm_Player':      0 owned,  5 rejected
wid:   'Wpn_PlySword01':  0 owned,  7 rejected
wid:   'crossbow_01':     0 owned, 43 rejected
wid:   'bolt_01':         0 owned, 43 rejected
wid: 92 baseline signature(s) survived EVERY hide
```

There is not a single `wa:` line in the log beyond the config banner.
**Nothing was ever adopted, so `WaDraw` has never executed.** The attachment
code in `weapon_attach.cpp` is entirely untested - it is not known to be
wrong, it is known to be unreached. That matters for how much of it we trust.

---

## 2. The diagnosis

### 2.1 The recorded population is world geometry, not the view model

The per-phase totals:

```
phase  0 baseline              :  89456
phase  8 Wpn_PlySword01 show   : 155323
phase  9 crossbow_01    HIDE   : 383181
phase 16 bolt_01        show   : 265781
```

89,456 draws in a 1.5 s phase is ~60,000 draws/second. A first-person crossbow
is a handful of draws per frame - order 450 in a phase. **Hiding it cannot
move a total of that size**, which is exactly what the totals show, and that
was the question the per-phase instrument was added to answer. It answered it.

The 16 largest baseline signatures are unanimous:

```
vb 2FA79AE0 ib 7EBFE700 bones 1 prim 216 verts 684 stride 12
vb 7DAFEFE0 ib 7EB60D40 bones 1 prim 214 verts 515 stride 12
... all sixteen: bones 1, stride 12
```

Meanwhile the hand mesh is `c6 x144` (**48 bones**) and the inherited config
line reports the weapon rig as `c6 x36` (**12 bones**):

```
config: hand render drive ON (arms=1 c6 x144, weapon=1 c6 x36, ...)
ms/palette: PER-CLASS | ... | c6 x144 (48 bones) | 105694 per-class draw(s)
```

Not one of the top sixteen is 48 or 12 bones. The view model is drowned in
world props at roughly 1000:1.

### 2.2 The view changed mid-sweep, which invalidates the run on its own

Seven of the top sixteen signatures drop to zero at phase 9 or 10 and never
return:

```
vb 2FA79AE0 ... | 5841 4473 3717 4148 4276 4320 4068 4068 1989 0 0 0 0 0 0 0 0
vb 7E69C320 ... | 2456 2192 3284 3500 3700 1980 1808 1808 1148 0 0 0 0 0 0 0 0
```

One explodes tenfold at the same moment (`... 2034 2034 7059 26123 17805 ...`),
and the phase totals quadruple. Something large changed in view around phases
8-9.

This is why `crossbow_01` and `bolt_01` show **43** rejects each while
`Skm_Player` and `Wpn_PlySword01` show 5 and 7: the later components' phases
land after the change, so every signature that died there reads as "vanished
on one hide but not the other". **A correct crossbow signature would have been
rejected too, had it been in the table.** The four-way test is doing its job -
it is refusing to call a scene change an identity - but it means the run is
void regardless of population, and nothing says so.

### 2.3 `bones` is a stale number and has been all along

`DcNotePalette` sets `g_dcPendingBones = count / 3` and `g_dcSinceUpload = 0`
on every c6 write. `g_dcSinceUpload` is incremented in exactly one place -
`DcNoteDraw` - which returns early on `!g_dcOn`. **The census is off by
default, so `g_dcSinceUpload` is permanently 0 and the reuse window never
closes.**

Consequences:

* `freshPalette` is always true, which is why the "no fresh palette" column
  reads 0 for all 17 phases. That instrument cannot fail and told us nothing.
* `s.bones` is not this draw's palette size; it is *the size of the last c6
  write, whenever that was*. Every `bones 1` above is a draw that happened to
  follow a 3-register write. The bone counts in all three reports are garbage.
* Widening the gate in `bc73daed` changed almost nothing, because the gate was
  already effectively `g_dcPendingBones != 0`. That commit's reasoning was
  right and its effect was near-nil.

### 2.4 What is NOT wrong

Worth stating so the reviewer does not re-litigate it:

* The hides work. Verified by flag readback and by eye, three runs running.
* The four-way hide/show/hide/show test is correct and is the reason no false
  positive has been shipped since the first one.
* Buffer identity (`vb`+`ib`) is the right key for a geometry.
* The report-from-either-lane fix (`5df74130`) works - this report printed.

---

## 3. Defects to fix whichever route wins

1. **`g_dcSinceUpload` never increments when the census is off.** Move the
   increment out of the `g_dcOn` early-return, or track it independently.
   Everything that reads `bones` is wrong until this is fixed, including
   `WaDraw`'s register count.
2. **`WaDraw` sizes its palette read from `w->bones`**, which is the stale
   number above. It must come from the c6 write that this draw actually
   follows.
3. **`WaDraw` hardcodes register 6** for both `GetVertexShaderConstantF` and
   the corrected upload. The layout reflection already reports the real
   register in `g_pcLayBones`, and the log shows different shaders draw this
   mesh. The hand path hardcodes 6 as well and works, so this is latent rather
   than active - but the weapon's shader has never been checked.
4. **A run whose view changed should be declared VOID.** Count baseline
   signatures that stop being drawn before the final phase; past a threshold,
   refuse to attribute and say why. Right now an unusable run produces the
   same "0 owned" as a clean one.

---

## 4. The two routes

### Route A - match the draw's LocalToWorld against the component's own (recommended)

**Drop the sweep for identification entirely.**

Each `FpCand` (`crossbow_01`, `bolt_01`, `Wpn_PlySword01`) is a live
`SkeletalMeshComponent` whose `LocalToWorld` is already readable from the
UObject: rows at `+0x60/+0x70/+0x80`, world translation at `+0x90`.
`FpComputePivots` and `FpWorldPos` read exactly these today.

Each draw already yields the same matrix from the shader: `MpAcquireCtx`
reads `c231` into `ctx.l2w` / `ctx.R_L` / `ctx.t`.

They are the same matrix, differing only in that the shader's copy is
camera-relative. So:

* **Identify on the rotation.** `ctx.R_L` must equal the component's basis to
  tight tolerance. Translation does not enter, so the camera offset is
  irrelevant.
* **Self-check on the translation.** For every matched draw in one frame,
  `component.worldT - ctx.t` must be *the same vector* - it is the camera
  position. An instrument that can fail its own hypothesis: if the residuals
  disagree, the match is wrong and the draw is refused.
* If rotation alone cannot separate the crossbow from a bolt parked parallel
  to it, use the recovered camera position to disambiguate on translation.

Why this is better:

* No 26 s blinking sweep, no standing still, no void runs.
* Re-acquires every frame, so a re-equip that reallocates buffers costs
  nothing - which the plan doc flags as a requirement the sweep does not meet.
* Immune to population size and to the view changing.
* Names a *component instance*, not a geometry - strictly stronger than what
  the sweep produces, and it is what the plan's "qualify a component-to-render
  -context link" asks for.

Risks:

* Assumes the shader's `LocalToWorld` is camera-relative world with an
  unrotated camera basis. `ms/palette/cmp` telemetry suggests this holds, but
  it should be asserted on the first run and refused if not.
* The component matrix is read on the script lane while the draw happens on
  the present lane. Needs a published snapshot per frame, the same pattern
  `g_mpPalmTarget` already uses.

### Route B - keep the sweep, but filter the population to the view model

Gate `WiNoteDraw` on first-person proximity: record only draws whose
`|ctx.t|` is under ~100-150 uu. World geometry is far away; the view model is
not. Optionally also gate on palette size once defect 3.1 is fixed.

That should cut 3.26 M draws to a few thousand, at which point the existing
four-way test has an unmissable signal.

Cheaper to implement than Route A and reuses proven code. But it keeps every
structural weakness: the tester must stand still for 26 s, a re-equip
invalidates the table, and it identifies a geometry rather than an instance.

### Recommendation

**Route A, with Route B's proximity filter kept as the sweep's population gate
so the sweep remains available as a cross-check.** Route A is the one the plan
document already asks for, and it is the only one that survives a re-equip.

Implement in this order:

1. Defect 3.1 (`g_dcSinceUpload`) - it is three lines and everything else
   reads that number.
2. Route A identification, with the translation-residual self-check, logged
   before anything is attached.
3. Only once identification is proven in a log: let it feed `WaAdopt`, and
   test the attachment - which, again, has never run.
4. Defects 3.2 and 3.3 as part of step 3.
5. Defect 3.4 if the sweep is kept.

---

## 5. Questions for the reviewer

1. Is the camera-relative reading of `c231` safe to rely on, or should Route A
   derive the camera position independently first and assert against it?
2. Route A matches on a 3x3 to some tolerance. What tolerance, and should it
   be an angle (`rotation_diff_deg`, already in `hand_frame.h`) rather than an
   element-wise epsilon?
3. Is there a cheaper per-draw discriminator than a rotation compare against
   every candidate? At 60k draws/second the compare runs a lot, though the
   proximity gate would cut it first.
4. `WaDraw` returns `true` and draws the weapon itself. Should it instead
   patch c6 and return `false`, letting the existing path draw? That would
   remove one copy of the draw call and one failure mode - but the palette
   would then need restoring after the fact, which is what the current
   structure avoids.
