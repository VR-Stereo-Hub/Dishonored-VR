# VR-138: filling the unmodelled side of the pistol and crossbow (plan)

Status: IMPLEMENTED 2026-09-18 (`src/game/dishonored/hands/weapon_mirror.cpp`),
shipped `[Mirror] Enabled=0` in code, not yet seen in a headset. Deviations from
the plan below, both deliberate: (1) the per-geometry state is a separate
8-entry table keyed by the draw's buffers, ranges, stride and asset, not a
WaMesh field - WaMesh is memset/evicted in several places and the table needs
one release hook (`WaInvalidateContracts`) and rebuilds on any key mismatch;
(2) the math lives in `hand_frame.h` (`reflection_3x4`, `mirror_palette_right`)
so `frame_test` pins it (`mirror_compose`, `mirror_can_fail`).

**Build464 result (headset): no visible change - both builds REFUSED.** The
cut-face rule of section 3 found no cut: `Wpn_PlyGunElite` best face xmin 26 of
6330 used vertices (opposite 5), `crossbow_01` ymin 8 of 1961 (opposite 6), both
under the 3% floor. These meshes are not half-models cut at a plane; they have a
symmetric core and miss detail on one side. Build465 replaces section 3's
measurement with a SYMMETRY SEARCH: for each axis, 33 offsets over the middle
80% of the extent then a 17-step refinement, scoring the fraction of sampled
vertices (<= 1500) whose reflection lands within 0.5 uu of a DIFFERENT vertex;
vertices within 0.5 uu of the candidate plane are excluded (they match
themselves). Accept the best axis at >= 0.20 and >= 1.15x the next axis; the
modelled half is the side with more vertices. `mirror/plane` logs every axis's
best score and offset, so a refusal still names a usable override.

**Build467 result (headset):** pistol mostly filled (plane x=0.09, kept 1109 of
2772), still missing some parts on the left and possibly the top; crossbow
unchanged - it is symmetric (x score 0.976), kept only 47 triangles, so its gaps
are ONE-SIDED faces, not a missing half. Build468: (1) a triangle is skipped as
"already modelled" only when its centroid AND all three mirrored corners land on
existing geometry (a single stray vertex skipped 1040 pistol triangles);
(2) `[Mirror] BackFaces=1` redraws the weapon with the cull flipped and the same
corrected palette, so a hole seen from the unmodelled side shows the far wall's
back face instead of nothing (closed surfaces hide it behind the front faces).
Seam: `mirror back on|off`.
Scope: `Wpn_PlyGunElite` (pistol) and `crossbow_01` (crossbow, 1961 verts per
VR-57-MODEL-RAY.md). The sword, the Heart and the other items are out of scope.

## 1. The idea

The weapons are skinned draws, and the mod already owns them. Every placed
weapon pass goes through `weapon_attach.cpp`, which rewrites the bone palette
(`patched[i] = delta * P[i]`, `MpBuild`), draws, then restores the palette. A
mirrored copy is one more draw inside that scope:

```
mirror[i] = delta * P[i] * S          S = reflection in REFERENCE-POSE space
```

S sits on the RIGHT of the palette, so it reflects the vertex buffer's own
coordinates BEFORE skinning. Every bone gets the same S, so the weighted blend
commutes (`sum w_i P_i (S v) = (sum w_i P_i) S v`) exactly as the rigid delta
does on the left (VR-33 section 1). Multi-bone weapons (the pistol's hammer,
the crossbow's arms) therefore mirror correctly, and the plane can be measured
once from the vertex buffer instead of per frame.

A reflection reverses triangle winding, so the mirror draw flips
`D3DRS_CULLMODE` (CW <-> CCW) and restores it afterwards.

## 2. Only fill what is missing

Mirroring the WHOLE mesh would z-fight wherever the model already has both
sides (grips, triggers, the crossbow stock). The mirror draw therefore goes
through OUR index buffer, holding only triangles whose mirror image lands where
the original has no geometry:

1. Signed distance `d(v) = n . (v - c)` to the plane (n unit, c on the plane).
   The modelled side is `d >= 0` (the plane choice guarantees that, section 3).
2. Build a hash of every used vertex with `d < -Eps` (the geometry that already
   exists on the unmodelled side), cell 1 uu.
3. Keep triangle T only if: all three vertices have `d >= -Eps`; T is not on
   the plane (`max |d| > Eps`, or it is a cut-face cap that would coincide with
   itself); and no hashed vertex lies within `FillRadius` of `S * centroid(T)`.
   `Eps` = 0.25 uu, `FillRadius` = 1.5 uu. Both are ini keys.
4. Log `kept K of N triangles`. K = 0 means nothing is missing (or the plane is
   wrong) and the mirror refuses.

## 3. The plane, measured from the vertex buffer

A reader, `WmBuildContract`, based on `BrReadGeometry` (bolt_model_ray.cpp)
but streaming (no 1024-vertex cap, no per-vertex arrays except the
kept-triangle output):

* Same guards as `BrReadGeometry`: TRIANGLELIST only, FLOAT3 POSITION in
  stream 0, stride/offset equal to the contract, index range inside the buffer,
  READONLY lock where the usage allows. The pointers from `GetStreamSource` /
  `GetIndices` are released inside the call; only the values are kept (VR-33
  section 5 rule).
* Pass 1 over the used vertices: the bbox, and for each axis a count of
  vertices within 0.1 uu of the min face and of the max face.
* The barrel axis is the longest bbox extent and is never the mirror axis.
  Of the other two axes and their two faces, the CUT FACE is the face with the
  densest vertex slab: an open half-model has an edge loop on its cut plane.
  Accept only when that count is at least 3% of the used vertices AND at least
  3x the opposite face on the same axis. Otherwise refuse and log
  `mirror/build: no cut face - set [Mirror] Plane_<asset>`.
* n = that axis, pointing into the modelled half. c = that face's coordinate.
* The override `[Mirror] Plane_<asset>=<x|y|z>,<offset>,<+|->` replaces the
  measurement entirely. Its log line says the plane was NOT measured.

Pass 2 builds the kept index list (section 2) into a `D3DPOOL_MANAGED`,
`D3DUSAGE_WRITEONLY` index buffer. That is the pattern `mesh_split.cpp` uses
for `g_msIb` (the `CreateIndexBuffer` near line 1539): 16-bit unless an index
exceeds 65535. The same original index values are used, so the draw keeps the
game's `baseVertex/minIndex/numVertices`.

Build once per contract, lazily, on the first placed draw of an allowed
asset. Store it on the `WaMesh` (new fields below) and drop it wherever the
contract is invalidated (`WaInvalidateContracts`, `WaRetireContractsNotIn`,
the menu-keep revalidation). Keyed by the vb/ib VALUES and the asset, so a
reused address with a different asset is a rebuild, not a reuse.

## 4. Code changes, file by file

**`src/game/dishonored/hands/weapon_mirror.cpp` (new, unity-included right
after `weapon_attach.cpp`'s state chunk, before `weapon_attach.cpp` itself so
the draw sites can call it; prototypes in `src/mod/fwd.h`)**

```cpp
struct WmContract {           // one per WaMesh, zero = not built
    uint8_t  state;           // 0 unbuilt, 1 built, 2 refused (no retry until rebuild)
    char     why[64];         // refusal reason, logged once
    float    n[3], c[3];      // plane, reference-pose space
    bool     measured;        // false = ini override
    IDirect3DIndexBuffer9* ib; // ours, MANAGED
    D3DFORMAT fmt; UINT prims; // kept triangles
    UINT     totalPrims;
};
static bool WmEnabledFor(const WaMesh* w);                 // lever + asset allowlist
static bool WmBuildContract(IDirect3DDevice9*, WaMesh*, WmContract*);
static void WmMirrorMatrix(const WmContract*, float S[12]);  // 3x4, S = I - 2nn^T, t = 2(n.c)n
static void WmComposeRight(const float* P, const float* S, float* out, UINT regs); // out_i = P_i * S
static bool WmDraw(IDirect3DDevice9* dev, WaMesh* w, const float* sourcePalette,
                   UINT boneReg, UINT regs, const dvr::hf::Xform& delta,
                   INT baseVertex, UINT minIndex, UINT numVertices);
static void WmRelease(WaMesh* w, const char* why);           // on contract drop and device loss
static bool WmCommand(const char* args);                     // the seam word
```

`WmDraw` steps, all inside the caller's patched-palette scope:
1. `WmEnabledFor(w)`, the contract built (`WmBuildContract` on first use), and
   `prims > 0`; else return false with no state touched.
2. `WmComposeRight(source, S, tmp)`, then `MpBuild(patched, tmp, regs, &delta)`,
   then `orig_set_vs_const(boneReg, patched, regs)`. The caller restores
   `source` after, as it already does.
3. Save `D3DRS_CULLMODE`. Set the opposite (NONE stays NONE).
4. Save the current index buffer (`GetIndices`, release immediately, keep the
   value), `SetIndices(ours)`, then `orig_draw_indexed(dev, D3DPT_TRIANGLELIST,
   baseVertex, minIndex, numVertices, 0, prims)`, then `SetIndices(saved value)`.
   The saved pointer is re-set without holding a reference across the draw. If
   `SetIndices(saved)` needs a live reference, take it with `GetIndices` and
   release it after the restore, all within this function.
5. Restore the cull mode. Count ok/failed in `g_wm*` counters.

**`src/game/dishonored/hands/weapon_attach.cpp`, three call sites, each after a
successful original draw, before the palette and viewport restore (so the
mirror shares the full depth range):**
* the main placed path (`orig_draw_indexed` near line 1474, after `BrMeasure`),
* `WaPatchAndDraw`, indexed branch only (the sibling depth/shadow passes use
  the identical delta, so the mirror follows them into every pass),
* NOT `WaDrawPrim` (non-indexed): no index buffer to substitute. Log once
  that a non-indexed pass of a mirrored asset was left unmirrored.

**`src/mod/state/57b_game_dishonored_hands_weapon_attach.inc`**: add
`WmContract mirror;` to `WaMesh`. Every place that zeroes or evicts a WaMesh
calls `WmRelease` first.

**Config (`config.cpp` defaults + golden, `commands.cpp`, `overlay.cpp`)**

| Key | Default | Meaning |
|---|---|---|
| `[Mirror] Enabled` | 0 | the lever; code default off, installed ini arms it for the test |
| `[Mirror] Assets` | `Wpn_PlyGunElite,crossbow_01` | exact asset names from `w->asset` |
| `[Mirror] Eps` | 0.25 | uu, on-plane tolerance |
| `[Mirror] FillRadius` | 1.5 | uu, "already modelled here" radius |
| `[Mirror] Plane_<asset>` | unset | override: `y,1.25,+` |

Seam: `mirror on|off|status|rebuild|plane <asset> <axis> <offset> <sign>`.
F10: a checkbox "Mirror pistol/crossbow" and a "Rebuild" button.

## 5. Logging (so one run answers the questions)

* `mirror/build: '<asset>' verts V used U prims N | bbox (..)-(..) | barrel
  axis A | cut face <axis><min|max> at <c> holding H verts (P%, opposite O) ->
  plane n=(..) c=.. MEASURED | kept K of N triangles (Eps, FillRadius)`.
  The refusal variant names the failed test with its numbers.
* `mirror/beat` every 5 s while armed: mirrored draws/s per asset, failures,
  and the cull mode it found (it must name the native mode).
* On a refusal to draw: the reason, once per spell (`DVR_LOG_EVERY_MS`).

## 6. Risks and how each is handled

* **Wrong plane**: the mirror floats beside the gun. The build line gives the
  numbers, and `mirror plane` fixes it live without a rebuild of the DLL.
* **Normal-map handedness**: the mirrored side's bumps light from the wrong
  side (the binormal sign comes from the vertex, not the palette). This is
  cosmetic and accepted for a first cut. It says so on the build line.
* **Depth pre-pass / shadows**: the mirror follows every pass the attach
  places, so depth and colour agree. Shadows gain the mirrored half, which is
  correct.
* **The model ray**: `BrMeasure` runs on the ORIGINAL draw before the mirror,
  and the mirror never publishes a delta. The ray is unchanged by construction.
* **Stereo**: the mirror draw happens inside the game's draw call, so the
  re-entry's second eye repeats it like any other pass.
* **Menus and loads**: the contract drop paths release our index buffer, and
  no draw happens without a placed contract in the current Present.
* **Device loss**: MANAGED pool survives a reset; `WmRelease` on device
  teardown mirrors `g_msIb`'s handling.

## 6b. Installed473 result and the hole-cap change (2026-09-18)

Tester: the pistol still misses a few areas; the crossbow is not visibly
mirrored; its right side flickers. The log explains the last two: the
crossbow's best plane is x 0.976, so both sides are already modelled. The 50
triangles the mirror kept were near-duplicates of existing faces (the symmetry
is 0.21 uu off-centre), which z-fight. The back-face pass on its thin two-layer
parts can z-fight the same way, and it showed no visible help in either run.
Installed476:
- `[Mirror] SymmetricSkip=0.90` (removed in installed480, see 6c).
- `[Mirror] BackFaces=0` in the installed ini.
- `[Mirror] Caps=1`, new: the missing areas sit where the hand covered the
  model in flat play, so they are expected to be OPEN holes. Boundary loops
  (welded at 0.02 uu so UV seams do not count) are closed with fans over their
  own vertices, culling off, in the weapon's own palette. Refused per loop:
  open chains, longer than 96 edges, wider than 45% of the model, and sheet
  outlines (a flat one-layer part whose neighbours lie inside the loop).
  `mirror/caps:` logs every count. Counterprediction: `boundary edges 0` on
  both weapons means the meshes are closed and the gaps are not holes (next
  suspects: culling on the reflected/cut draw or the hand cut hiding weapon
  triangles).

## 6c. Installed476 result: the symmetric-skip was wrong (2026-09-19)

Tester: the crossbow's whole left side is invisible, and the pistol still misses a
little. So `SymmetricSkip` was wrong: the crossbow's vertices are 97.6% symmetric,
but its faces are not. The unmodelled side keeps its vertices (edges and the
thickness of the other side's plates) with no faces looking out of it. The
vertex-only occupancy test had the same blind spot, and that is why the
installed470/473 copies kept only 50 triangles. Installed480:
- SymmetricSkip removed.
- The modelled side is the one with more area facing OUT of it; the normals'
  outward sign is measured against the mesh centre, so winding is not assumed.
- A mirrored triangle is skipped only when its centroid and all three corners
  land on existing surface facing the same way (normal dot > 0.5).
- `mirror/facing:` logs both sides' outward area and the chosen side.
Hole caps stay on: run476 capped 58 crossbow loops (388 triangles) and 25
pistol loops (165 triangles).

## 6d. Installed480 result: crossbow filled, pistol worse (2026-09-19)

Tester: the crossbow is much better, with only a few mirrored areas missing (the
tester checked the other side); the pistol misses MORE than in 476; no flicker on
either. Run480 (logs in build/playtest-candidates/wheel-blackout/run480): both
modelled sides were chosen as -x by outward-facing area (pistol 526.7 vs 244.2,
crossbow 366.5 vs 149.3). Kept 1352 of 2772 on the pistol (797 counted as
already modelled) and 120 of 1957 on the crossbow (767). The pistol regression is
consistent with the 480 coverage test: a same-facing sample point within
FillRadius (1.5 uu) counted as modelled, so small raised parts next to a flat
face were skipped. Installed482: coverage is the true point-to-triangle distance
within `[Mirror] CoverTol` (0.3 uu), on a same-facing triangle, for the
centroid and all three corners. Counterprediction: the "already modelled" count
drops on both weapons, and the missing areas shrink without a flicker returning.
If flicker returns, the copies now land within 0.3-1.5 uu of real surfaces;
lower CoverTol or raise it until it stops.

## 7. Verification

1. Build, lint, golden. `frame_test`: add `mirror_compose_commutes` next to
   `compose_commutes` in `hand_frame_test.h`: for random P, S reflection, D
   rigid: `D*(P*S)` applied to v equals `D*P` applied to `S*v`, and `det < 0`.
   Also `mirror_can_fail`: a non-reflection S must fail the det test.
2. Headset, one question: with the pistol and then the crossbow in hand, turn
   the weapon to see its far side. Is it solid, with no flicker on parts that
   were already there? Log check: two `mirror/build` lines with MEASURED planes
   and K > 0, and `mirror/beat` counts about equal to the weapon's draw rate.
