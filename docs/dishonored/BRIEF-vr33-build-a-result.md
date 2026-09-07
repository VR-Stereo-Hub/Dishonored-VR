# BRIEF 2: VR-33 Build A result - absolute placement tracks, and swings with the head

**Date:** 2026-09-07. **Branch:** `claude/vr-33-hands-and-weapons-at-the-controllers`
**Head:** `04a192f0`. **Follows:** `BRIEF-vr33-palette.md` and Codex's
`VR-33-PALETTE-REVIEW.md`.

**Nothing has been changed in response to this result.** The diagnosis below is
derived and supported by logged numbers, but the fix is not written, not built
and not installed. This is for review before the next build.

---

## 1. What was adopted from the review, and what happened

Codex's Build A was implemented as specified: a fixed palm anchor of vertex
identities, skinned each frame from the game's own unmodified palette, with

```
translation = target - q(t)
q(t) = sum_j a_j sum_i w_ji M[idx_ji] v_j
```

Anchors selected cleanly: **12 vertices on class A (left), 11 on class B
(right)**, from 1621 triangles each.

### One dead run first, worth recording as a process failure

The first absolute run reported "hands stay put, but no controller influence at
all". That was not a result. `MpDriveTick` opened with
`if (!g_mpDrive) return;`, and absolute mode had been armed with
`PaletteDrive=0`, so no controller position was ever published, `g_mpCtlOk`
stayed false, and the draw fell through to the engine's own hands.

The engine's own hands are still hands, so a lane that never ran looked like a
feature that does not work. The log showed it immediately - anchors selected,
zero `CALIBRATED` lines, zero beat lines - but only because someone went
looking.

**This was the second time in the session that a tick was hidden behind another
feature's flag** (`MsTick` sat below `DcTick`'s `if (!g_dcOn) return;`, so
disabling the draw census silently killed the mesh split's entire tick and put
the arms back on screen). Both are now fixed, and the silent fall-through in
the draw has been replaced by a warning that names which input was missing.

### The real result

With the gate fixed: **the hands track the controllers.** Absolute placement
works as a mechanism.

**But they now move opposite head movement on all three axes, including
vertically.** Under the previous relative drive the vertical axis was correct;
absolute placement made it worse.

---

## 2. The diagnosis, derived

Let `c`, `h` be controller and head positions in tracking space, `R` the head
rotation, and `B` the measured translational basis (a componentwise negation:
palette left/down/forward against XR right/up/backward).

The implementation computes, per hand:

```
c_pal  = k * B * R^T * (c - h)          the controller, head-relative, palette axes
o      = q(t0) - c_pal(t0)              calibrated once, stored as palette coords
target = c_pal + o
T      = target - q(t)                  so the palm lands exactly on target
```

The palm therefore sits at `c_pal + o` in palette space. Mapping back to the
world, and using the fact that `c_pal` is just `(c - h)` re-expressed in the
head frame:

```
world_palm = h + R * ( (c - h)_head + o_head )
           = h + (c - h) + R * o
           = c + R * o
```

**`R * o` is the bug.** `o` is stored as a constant in palette coordinates, but
the palette frame rotates with the head (Test 7). A vector held constant in a
head-rotating frame sweeps a circle in the world as the head turns. The palm is
placed at the controller *plus a rigid arm of length `|o|` that pivots with the
head*.

This is precisely the failure mode the review named in advance:

> Calibrate a fixed grip-to-palm transform per side. Its translation rotates
> with the controller; it is not a fixed camera-space positional correction.

`o` was implemented as exactly that forbidden thing - a fixed camera-space
positional correction.

### The magnitude, from the log

```
hand 0 CALIBRATED - palm at (22.7 -118.9 54.7) uu, controller at (12.6 18.9 36.0) uu,
                    constant offset (10.2 -137.8 18.8)
hand 1 CALIBRATED - palm at (-31.6 -105.5 17.2) uu, controller at (-11.0 22.4 38.4) uu,
                    constant offset (-20.6 -128.0 -21.2)
```

`|o|` is about **139 uu on the left and 131 uu on the right**, dominated by the
axis-1 (down) component. At the configured 100 uu/m that is roughly **1.3-1.4
metres**, almost entirely vertical.

A 1.4 m lever pivoting with the head is a very large swing, and it explains why
absolute placement made the symptom worse rather than better, and why the
vertical axis broke when it had previously been correct: the relative drive had
no `o` term at all.

---

## 3. What the offset's size implies about the palette's origin - conjecture

The calibrated offset is ~1.4 m, and its sign puts the palm **above** the
palette's origin (axis 1 is DOWN, and the palm's axis-1 coordinate is about
-119 while the controller's is about +19).

If the palette's output space were camera-relative, a palm 1.4 m above the
camera would be absurd. A ~1.3-1.4 m vertical separation is close to eye
height, which is consistent with the palette's origin being at or near the
**pawn root (the feet)** rather than the camera.

**This is inference from one number and is not established.** It matters
because it means `c_pal` (a head-relative vector) and `q` (a palette-origin
vector) do not share an origin, and `o` was silently absorbing that mismatch as
well as any genuine grip offset. Those are two different quantities and
conflating them is what produced a 1.4 m constant.

The axis test measured the palette's *directions*. It never measured its
*origin*, which the review explicitly said would need establishing in the same
build. That was not done, and this is the cost.

---

## 4. Candidate fixes, none implemented

The correct target is

```
target_pal = H_pal(t) + v_pal(t)
```

where `v_pal` is the controller's offset from the head in palette axes (already
computed correctly) and `H_pal(t)` is the **head's position in palette
coordinates**. The open question is entirely about `H_pal`.

**Option 1 - counter-rotate the calibrated offset.** Store `o` as a world (or
body) vector and re-express it in palette coordinates each frame. This removes
the swing, and yields `world_palm = c + o_world`: the palm tracks the
controller rigidly with a fixed world offset. But a *fixed 1.4 m world offset*
is also wrong visually - the palm should be at the controller, not 1.4 m above
it. This treats the origin mismatch as if it were a grip offset.

**Option 2 - derive `H_pal` properly and set the grip offset to zero.** If the
palette origin can be established (camera position via `c5`, the pawn root, or
measured directly), then `H_pal` is computable each frame and the palm goes to
the controller's true position. This is the honest fix, and it requires
actually measuring the origin rather than absorbing it into a constant.

**Option 3 - anchor the head the same way the palm is anchored.** The palm's
position is obtained by skinning real vertices rather than trusting any
assumption about the palette. If a vertex set near the head or neck existed in
the same palette, `H_pal` could be measured the same way, with no origin
assumption at all. The view model may not contain such geometry - unverified.

**Option 4 - sidestep the origin entirely.** Only *changes* in `q` and in the
controller are needed to hold the palm on the controller if the mapping is
already correct. This risks reintroducing the relative drive's error, which is
why it is listed last and not preferred.

---

## 5. Questions for review

1. **Is the derivation `world_palm = c + R * o` correct**, and is `R * o` with
   `|o|` about 1.4 m sufficient to explain a head-coupled swing on all three
   axes? Or is there a second coupling still hiding behind it?
2. **Is the pawn-root origin conjecture worth acting on**, or should the origin
   be measured directly first - and by what test that cannot answer itself?
   The session has already produced one instrument that could not fail.
3. **Which option above?** Option 2 looks correct but needs the origin. Option
   3 avoids every assumption but may have no geometry to stand on.
4. **Is `k = 100 uu/m` right?** The review flagged it as unvalidated and it has
   still not been checked at two depths. If the scale is wrong, the palm
   tracks with a gain error that no origin fix will remove.
5. The review's palette-validity, `startIndex`/stride contract and pose-timing
   corrections are **still not implemented**. Should they land before or after
   the origin fix? They are candidates for dynamic error but not for the fixed
   1.4 m lever.

---

## 6. State of the tree

| Item | State |
|---|---|
| Absolute placement | Implemented, armed, `[Hands] PaletteAbsolute=1` |
| Relative drive | Implemented, off (`PaletteDrive=0`), superseded |
| Axis probe / timed sweep | Implemented, off |
| Frame probe | Implemented, off, **withdrawn as unsound** |
| Palette validity contract | **Not implemented** |
| startIndex / stride contract | **Not implemented** |
| Pose timing alignment | **Not implemented** |
| Build B (orientation, grip) | Not started |
| Build C (weapons) | Not started |

Nothing merged to `VR-Main`. The ENGINE_NOTES wording about the native route
has also not yet been softened to "measured, and parked" as the review asked.
