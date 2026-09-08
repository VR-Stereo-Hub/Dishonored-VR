# PLAN: VR-33 step 2 - recover the draw's own coordinate conversion

**Date:** 2026-09-07. **Branch:** `claude/vr-33-hands-and-weapons-at-the-controllers`
**Head:** `c2cd3a0b`. **Follows:** `VR-33-BUILD-A-REVIEW.md` sections 4A and 4B.

**Nothing in this plan is implemented.** This is the design, for review before a
line is written. Step 1 (anchor, refusal, residual, palette cache, draw
contract) is committed at `c2cd3a0b` and described at the end.

The goal: stop inferring where the palette's origin is and **measure the
conversion the draw actually uses**, then place the palm through it.

---

## 0. What this must not do

Three constraints shape every decision below, and two of them are project rules
that already have scar tissue behind them.

- **Shader bytecode is game-derived content.** `.gitignore` already excludes
  `dumps/`, `*.dmp` and `tools/uscript/*` for this reason. Captures land in
  `<data_dir>\dumps\` and nothing derived from them is committed - only
  conclusions, in `ENGINE_NOTES.md`, in our own words.
- **Never take a reference to an engine D3D object inside a detour.**
  `GetVertexShader` and `GetVertexDeclaration` return AddRef'd pointers. Every
  one is released before the detour returns, on every path including refusals.
  The existing contract check in `MsDraw` already does this and is the pattern
  to copy.
- **An instrument that cannot fail its own hypothesis is not evidence.** This
  session already produced one. Section 5 states, for each step, the result
  that would falsify it.

---

## 1. The problem, stated without the disputed parts

Build A places the palm at `target_palette` and the palm lands there. What is
not known is what `target_palette` **means** in the world, because the
conversion from palette coordinates to the rendered image has never been
measured. The review's formulation:

```
p_world = O(t) + L(t) * q_palette
```

`O` and `L` are both unmeasured. The calibrated offset `o` absorbed `O`, any
genuine grip offset, any scale error and any anchor error into one constant,
which is why it could not be interpreted - and why the left and right values
differ by 51.4 uu when a shared palette frame cannot have a per-hand origin.

The fix is not another offset. It is to recover `L` and `O` from the draw.

---

## 2. Stage A: make one qualified hand draw self-describing

### A1. Qualify the draw

A capture is only meaningful if we know it came from the draw we care about.
Trigger only when **all** hold, inside `DcDrawIndexed`
(`draw_census.cpp:220`), which already receives `startIndex` and the full
range:

- `DcIsLocked(self, true)` - the split's own mesh identity (VB/IB), and
- the full `MsDraw` contract now passes (base vertex, min index, vertex count,
  `startIndex`, stream-0 offset, **stride**, declaration), and
- a palette is current and passed the qualification added in step 1, and
- the frame/eye is recorded, so a capture is never silently mixed across eyes.

### A2. What to capture, once per distinct shader identity

Capture is **one-shot per shader pointer**, not per draw, and is written from a
worker rather than in the render path:

| Item | Source |
|---|---|
| Vertex shader bytecode | `IDirect3DVertexShader9::GetFunction` (two-call: size, then buffer) |
| Constants actually in force | `GetVertexShaderConstantF` over the ranges that matter |
| Declaration elements | `IDirect3DVertexDeclaration9::GetDeclaration` |
| Draw range | base vertex, min index, num vertices, `startIndex`, prim count |
| Stream 0 | buffer pointer, offset, stride |
| Frame context | frame number, eye tag, viewport, pose generation |

Every call's `HRESULT` is checked. **A failed read is not zero state** - it
aborts the capture and logs why, because a zeroed constant block that looks
like a valid matrix is exactly the kind of plausible-and-wrong input this
project has been bitten by.

Files: `<data_dir>\dumps\vs_<hash>.bin` plus a sidecar `.txt` of the numbers.
Gitignored. Disassembly and any file writing happen off the render thread.

### A3. Which constants

Not just c0. The review is explicit that assuming c0 is the whole map because
an older experiment labelled it "view-projection" is precisely the error to
avoid. Capture c0..c95 (bounded, cheap once) and let the bytecode say which
registers the skinning-to-clip path actually reads.

**A specific hazard, already in our own code:** `hkSetVSConstF` modifies c0 for
head and positional correction (`vs_const_hook.cpp:307-341`). A map
reconstructed from what the game *requested* is therefore not necessarily the
map the draw *used*. `GetVertexShaderConstantF` reads the device's current
state, which is what the draw consumes - that is the reason to prefer it over
our own cache. Runs used for this measurement should additionally have the
competing writers disabled so the two agree.

### A4. Reading it

Offline, from the captured bytecode: identify the skinning block (`c6+`
addressing via blend indices), the weight arithmetic, and every term applied
between skinning and the clip-position output. Vertex shader 3.0 assembly is
small and readable; no disassembler dependency is needed for a first pass, but
the encoding must be decoded properly rather than pattern-matched.

**The weight question step 1 deliberately left open is answered here.** If the
shader does not normalise, then a palette translation `T` moves a vertex by
`wsum * T`, and `target - q` does not produce the displacement it claims. Step
1 refuses the anchor when `|wsum - 1|` exceeds tolerance so this cannot pass
unnoticed; the bytecode says whether that tolerance is the right test at all.

---

## 3. Stage B: recover the optical centre

If, and only if, the post-skinning path is verified as a standard perspective
map, write it as

```
clip = Q * [q_palette, 1]
```

with `Q` composed of **every** verified affine term plus the projection - not
an assumed factorisation. Then the camera centre in palette coordinates is

```
centre_h = inverse(Q) * [0, 0, 1, 0]
centre   = centre_h.xyz / centre_h.w
```

Guards, all of which must be checked rather than assumed: invertibility,
conditioning, and a finite non-zero `w`. **An orthographic or non-standard path
yields `w = 0` and must be reported as "not recoverable", never forced into a
plausible centre.** The review verified this formula on 12 synthetic
perspective maps to 2.9e-14, with the orthographic case correctly degenerate;
that validates the mathematics, not this game's shader.

This identifies a **render optical centre**. It is not a camera bone and not
proof of any anatomical origin - which is exactly why it replaces the pawn-root
conjecture rather than confirming it.

Per eye: for an eye draw this is that eye's centre, so the matching
eye-relative controller vector must be used. Adding stereo displacement twice,
or substituting a head-centre pose without accounting for the difference, is a
named failure mode.

---

## 4. Stage C: place through the recovered conversion

With `F` mapping palette coordinates into the draw's view coordinates:

```
target_palette = inverse(F) * controller_view
T              = target_palette - current_skinned_palm_anchor
```

Changes from Build A:

- **The `q(t0) - controller(t0)` calibration is deleted.** Absolute alignment
  may legitimately jump away from the game's resting pose when it engages;
  that is correct behaviour, not a fault to be calibrated away. F6 stops being
  required for positional stability.
- **Grip offset is zero for this build.** It belongs to Build B and rotates
  with the controller, not with the camera.
- **The origin belongs to the draw and is shared by both hands.** A per-hand
  origin was a category error; per-hand corrections belong to the grip.
- **Scale is established, not assumed.** `[Hands] WorldScaleUU=100` and
  `[PosTrack] Scale=108` are both live in this build and at most one can be
  right for this conversion. Both get logged, and known distances are checked
  at two depths. Scale must not be fitted from the 139 uu calibration number.

---

## 5. Stage D: what would falsify each step

Simulator first, per the review, and each check names the outcome that kills it.

| Check | Falsified by |
|---|---|
| Marker rendered at the computed source point, drive OFF | It does not sit on the palm patch, or moves when fingers animate |
| Residual re-skinned from the submitted palette | Non-zero, or scaling with `wsum` |
| Recovered centre | `w` near zero, ill-conditioned `Q`, or a centre that moves when only the controller moves |
| Head rotation isolated (yaw, pitch, roll separately) | Palm moves in world |
| **Head translation isolated - never yet tested** | Palm moves in world |
| Controller motion isolated | Palm does not follow, or follows with a gain error |
| Animation moving, controller still | Palm moves |
| Both hands | Different origins needed |
| Reset, tracking loss | No explicit fallback |
| `PaletteAbsolute=1` with `PaletteDrive=0` | Regression of the feature-gate bug |

The last row is a harness test, not a headset test. That gate bug cost a run
and it is the second of its kind this session; it should be caught by the
harness, not by a person in a headset.

**Held-out poses only.** A conversion validated at the point it was derived
from is not evidence. The comparison target must come from an independent
XR-to-view path, not from the same formula being tested.

---

## 6. Cost, and the honest alternative

This is the largest single piece of work in VR-33 so far: bytecode capture,
an offline read of the skinning path, a projection recovery with real
degeneracy handling, and a simulator matrix before any headset time.

The alternative is to keep adjusting offsets against perceptual reports. This
session has now spent five runs that way and produced two retractions and one
withdrawn instrument. The measurement is cheaper than the next five runs.

**What this does not touch:** the native SkelControl route stays parked, with
its wording to be corrected to "measured, and parked" rather than
"impossible" - the sweep measured 66 controls not advancing, which is not the
same claim. No further global object sweeps.

---

## 7. Step 1, already committed at `c2cd3a0b`

For the reviewer's reference, since it changes what the next capture measures:

- Anchor is now the deduplicated vertices nearest the class's bind-pose
  centroid, with the patch radius logged. Was a triangle-order sample across
  the whole hand, fingers included, with possible duplicates.
- `MpAnchorPos` refuses on any invalid influence, any non-finite result, and
  any weight sum outside tolerance. It previously skipped bad vertices and
  returned the average of the rest as success, so a short palette silently
  changed which point was measured.
- The unconditional divide by the weight sum is gone.
- The residual is re-skinned from the palette actually submitted, not
  assigned zero.
- The palette cache accepts any block covering c6, extracts the palette window
  from a wider upload, and qualifies the length against the split's bone count.
  It previously demanded an exact c6 start and accepted `count >= 3`.
- `MsDraw` now receives and compares `startIndex` and the stream stride.
- The scale conflict is logged on every beat.

The placement model is unchanged, so a headset run against `c2cd3a0b` alone
would not be informative.
