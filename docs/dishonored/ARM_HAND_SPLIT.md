# The arm/hand split

How the mod draws Corvo's hands without his arms, and everything needed to
re-tune, re-derive or turn it off. This is the reference for `VR-31`; the
research that closed the other routes is in `ENGINE_NOTES.md` (search
"VR-31 research review"), and this file starts where that one ends.

Modules: `src/game/dishonored/hands/draw_census.cpp` (the instrument),
`src/game/dishonored/hands/mesh_split.cpp` (the split), state chunks 55 and 40.

---

## 1. The problem, in one paragraph

The arms and the hands are **one skinned triangle list**, drawn by one call,
with one material. There is no per-bone visibility flag that hides them, no
material section that separates them, and no draw that draws only the arms.
That was not assumed - it was measured, and the routes are closed one at a time
in ENGINE_NOTES with the instrument that closed each. So the only thing left is
to cut the geometry ourselves, which is what this module does.

## 2. What it does, in order

| Step | Function | What it produces |
|---|---|---|
| Read | `MsRead` | the draw's index range and vertex window, copied out of the game's buffers ONCE, and validated |
| Bones | `MsBones` | per-bone total weight, weighted centroid, and the co-influence graph |
| Sides | `MsSides` | the bones split into two arms - by the graph if it says two limbs, by the widest coordinate gap if it does not |
| Wrist | `MsWrist` | per side, the hand bone, and a wrist radius derived from the gap in the bone spacing |
| Axis | `MsBoneAxis` / `MsPca` | the direction the cut plane is square to |
| Classify | `MsClassify` | every triangle into handA / handB / armA / armB / unclassified, clipping the ones that straddle the plane |
| Cap | `MsCaps` | a disc closing each cut end |
| Upload | `MsUpload` | our own index buffer (and vertex buffer), one contiguous range per class |

Everything after `MsRead` is arithmetic on the copy, so moving the ring costs a
reclassify and a buffer refill - no second lock of an engine buffer.

## 3. The three shapes the cut went through

Each replaced the last for a measured reason, and each earlier one is still
reachable, because a shape that is one ini key away is a free fallback.

**A sphere around the hand bone** (`WristPlane=0`). It works, and it moves in
whole bones: the tester's walk showed nothing changing at all from scale 0.50
to 0.67, then 156 triangles per arm flipping in a single press. Its edge is the
blobby outline of one bone's influence region, not a cut.

**A plane perpendicular to the forearm** (`WristPlane=1`, `WristEdge` 0/1/2). A
plane cuts a cylinder in a circle and moves continuously - a press moves it a
couple of percent of the arm's length. This fixed the jumps and not the
outline: a triangle straddling the plane was still kept or dropped whole, so
the boundary was a sawtooth one triangle high, which on the coarse cuff
geometry reads as spikes hanging off the wrist. No edge RULE fixes that;
keeping only triangles entirely past the plane trades spikes for notches.

**The plane, with straddling triangles CLIPPED at it** (`WristEdge=3`, the
default). The boundary stops being made of original triangle edges and becomes
the plane itself. This is the only construction that gives an exactly circular
cut.

## 4. The axis: bone or PCA

A ring square to the wrong direction is slanted across the forearm, which is
what "it takes too much on the other side" looks like. Two candidates are
computed every rebuild and the angle between them is logged:

* **bone** (`WristAxis=0`, the default) - the hand bone minus the bone it hangs
  off, that bone being the hand's graph neighbour whose centroid is farthest
  away. No names and no hierarchy assumed, same as everything else here.
* **pca** (`WristAxis=1`) - the longest direction of the arm's own triangle
  cloud. A tapered sleeve leans this off the real bone.

Which is right is a question about this asset, not about geometry, so both
exist and `ms axis bone|pca` switches live. The default is the bone because it
is the anatomical answer.

## 5. The clip

A triangle crossing the plane is split into the part on each side, with new
vertices interpolated along the two crossing edges. Both halves are emitted
into their own class, so **`arms` stays the exact inverse of `hands`** and
`all` still rebuilds the original mesh.

That needs a VERTEX buffer of ours as well as an index buffer, because the new
vertices do not exist in the game's. Every stream-0 element is interpolated by
its declared type. Two things are deliberately NOT interpolated:

* **Blend indices** come whole from the nearer parent vertex, because a bone
  index is a name and not a quantity. Averaging bone 7 and bone 9 gives bone 8,
  which is a different bone.
* **Blend weights** go with them, so they cannot end up naming the wrong bones.

It also needs stream 0 to be the **only** stream, because re-basing the indices
onto a buffer of ours would desynchronise any second stream addressed by the
same index. That is checked, and a mesh with more streams falls back to the
whole-triangle rule with the reason in the log.

### The winding trap, paid for once

`MsClipTri` picks the odd vertex `i` and its neighbours as `(i+1)%3` and
`(i+2)%3`. Picking the three corners by ASCENDING INDEX instead reverses the
triangle whenever the odd vertex is the middle one - **one case in three** -
and a reversed triangle is culled. The symptom was two thirds of the cut coming
out on a clean line and one third missing or floating loose. If the edge ever
goes ragged again in that specific pattern, this is the first thing to check.

## 6. The cap

A clipped shell is an OPEN shell: the arm is a tube with nothing inside it, so
the clean circular boundary let you look down the inside of the sleeve and out
through its far wall. The clip already produces exactly the ring that has to be
closed - one boundary segment per straddling triangle - and a fan from that
ring's centre to those segments fills it.

* **Both stumps are capped**, into their own class, so hands-only closes the
  hand and the inverse view closes the sleeve. In `all` the two caps are
  coincident, face away from each other and sit inside a closed mesh, so they
  are invisible and the A/B still rebuilds what the game drew.
* **Two-sided by default** (`CutCapTwoSided=1`), and not as insurance against a
  winding mistake: in hands-only mode the arm is not drawn at all, so nothing
  stands between the eye and the BACK of the hand's cap. A one-sided disc would
  be culled from exactly the angle the hole was visible from.
* **The colour is the ring's own.** Every cap vertex is given the SAME texture
  coordinate - the MODE of the ring's UVs, counted by how many ring vertices
  sit within a sixth of the ring's UV spread of each candidate. A mode and not
  a mean: a mean lands between two islands of an atlas and samples whatever is
  parked there. One UV on every vertex means the disc interpolates to a single
  texel, so the cap is flat in the colour that dominates the cut - sleeve on
  the sleeve, skin past the cuff - and it re-derives on every knob press, so it
  tracks the ring by construction.
* **Position and skinning come from the ring vertex**, not from the colour
  donor, so the cap deforms with the arm and cannot part from the rim.

### The UV trap, also paid for once

The first build asked for a `FLOAT2` TEXCOORD0 and the log answered **NO
TEXCOORD0**, so the mode never ran and every cap silently fell back to ring
vertex 0. It looked right, because vertex 0 is still a ring vertex - and it
would not have on a ring landing on a texture seam. This asset packs its
texture coordinate as `FLOAT16_2`. A refusal that produces a plausible picture
is the worst kind of refusal, which is why the decode was widened rather than
the check loosened, and why the log line now names the offset and type it found
and says which of the two paths ran.

## 7. Reading the game's buffers safely

`D3DUSAGE_WRITEONLY` says the application will not read, and a driver is within
its rights to hand back an uninitialised staging copy. So every read is
validated against facts the data MUST satisfy:

* indices inside the draw's own vertex window
* bone indices below the palette size
* weights summing to 1
* positions finite

A failed validation **refuses and logs the numbers** instead of drawing a mesh
carved out of noise - which would look like a bug in the geometry rather than a
bug in the read. The buffer descriptors (usage and pool) are logged BEFORE the
lock, so the first run of a new driver says which world we are in.

On this asset, measured 2026-09-06: the vertex buffer is `usage=0x0 pool=0
size=88672 stride=32`, i.e. **not** write-only, and the lock returns real data.

## 8. Configuration

All under `[Hands]`. Every one of these ships at a value that has been through
a headset.

| Key | Default | What it does |
|---|---|---|
| `ArmSplit` | 1 | the module at all |
| `ArmSplitAuto` | 1 | arm the mesh lock from the signature, not from a key press |
| `ArmSplitMode` | 1 (HANDS) | 0 off, 1 hands, 2 all, 3 arms, 4 unclassified |
| `ArmMeshPrims` / `ArmMeshVerts` | 4448 / 2771 | the mesh signature the auto-arm matches |
| `WristPlane` | 1 | plane (1) or sphere (0) |
| `WristEdge` | 3 | 3 CLIP, 2 any vertex past, 1 all three past, 0 centroid |
| `WristAxis` | 0 | 0 bone, 1 PCA |
| `WristCutA` / `WristCutB` | -4.9 | **the ring position**, in mesh units from the hand bone, positive toward the fingers |
| `WristScaleA` / `WristScaleB` | 0.70 | the sphere's multiplier, and the seed the plane starts from |
| `WristStep` | 1 | knob step: 0 coarse (2 %), 1 fine (0.5 %), 2 ultrafine (0.1 %) |
| `CutCap` | 1 | close the cut end |
| `CutCapTwoSided` | 1 | cap faces both ways |

**`WristCutA` / `WristCutB` = -4.9 is a MEASURED number**, not a derived one and
not a guess: it is where the tester left the ring after walking it in the
headset on 2026-09-06, read straight off the `ms/wrist` line that press
printed. It is asset-relative - a distance from the hand bone along the limb
axis - so it survives a level load and does not depend on where the pawn is
standing. Deleting the key falls back to deriving the position from the seeded
sphere, which is the escape hatch if a future asset makes -4.9 wrong.

## 9. Hotkeys

| Key | Effect |
|---|---|
| Numpad 0 | cycle the mode (off / hands / all / arms / unclassified) |
| Numpad + / - | move the ring up or down the arm; auto-repeats after 400 ms |
| Numpad . | cycle the step size (coarse / fine / ultrafine) |
| Numpad * | pick which arm the knob moves (both / A / B) |
| Numpad / | re-derive the axis and the plane without re-reading the buffers |

The auto-repeat exists because at the ultrafine step the ring moves 0.1 % of
the arm per press, and placing it by hand would otherwise be a hundred taps.

## 10. Reading the log

Every line is `[hands] ms:` or `ms/`.

**`ms: decl stream0 - POSITION off=... BLENDWEIGHT off=... BLENDINDICES off=...`**
The declaration, with the D3DDECLTYPE numbers spelled out. If a later line
refuses on an encoding, this is the line that says what it found.

**`ms: stream 0 is the only stream, N bytes a vertex, M element(s)`**
The clip is available. The alternative names the stream that vetoed it.

**`ms: triangles by class - handA .. handB .. armA .. armB .. unclassified ..`**
The whole result in one line, plus how many source triangles were CUT and how
many new vertices that made, and which rule produced it. `unclassified` should
be 0 or near it; a large number means the side split is wrong.

**`ms: cut end CAPPED ...`** Names the colour path that actually ran (the mode,
or the arbitrary fallback), the TEXCOORD0 offset and type it found, the winning
ring vertex per side, and whether the normal was forced flat or inherited.

**`ms/wrist: MORE/LESS kept as hand - the ring is now X from the hand bone on
side A and Y on side B`** X and Y are exactly what `WristCutA` / `WristCutB`
take, so a look worth keeping can be made the default without another walk.
This is the line to read after tuning.

**`ms: beat - mode M | N draw(s) served from our index buffer, K fell through`**
Both at zero means the locked mesh is not being drawn at all, which is **normal
in a menu or a cutscene** - the line says so, because a zero that is expected
by design and a zero that means the instrument is dead must not look alike. A
rising fallback with a ready split means the draw did not match the one the
split was built from.

**Every `REFUSED` line carries the numbers that produced the refusal.**

## 11. Traps and things not to repeat

1. **The mesh lock must be armed by identity, not by a key press.** It was
   armed from a censused row once, and a level load that recreated the buffers
   left it pointing at freed pointers and silently drawing everything. It ships
   with `ArmSplitAuto=1` matching the mesh's own measured shape.
2. **A percentage mask cannot work for both arms.** The two arms occupy
   different, non-aligned regions of the one triangle list. The tester's mask
   `0x3001D237` worked on the left hand and took too much of the right; a
   second eyeballed mask would only have moved the problem. That is why the
   classification is by bone influence.
3. **Single-matrix draws saturate a draw census.** They were excluded before
   the census could be trusted.
4. **Hiding a PASS is not hiding a mesh.** The other passes draw the same
   geometry and the result reads as "nothing happened".
5. **The cut is computed in the BIND pose and seen in the ANIMATED one.** If
   the ring's shape CHANGES as the arm moves, that is the cause of a slant and
   the fix is a different one; if the slant is fixed relative to the arm
   whatever the pose, it is the axis. A tester can settle that in one run and
   nothing else can answer it.
6. **Everything here runs on the render thread**, inside the draw detour,
   because that is the only place the right buffers are bound. The hotkeys post
   requests; nothing acts on them off-lane.
7. **No engine D3D object is referenced outside the call that read it**, and
   nothing is written back to the game's buffers. The copy never leaves memory
   and is never written to disk.

## 12. What is still unverified

* The cap has not been through a headset since the UV decode was widened, so
  the cap's colour may now differ from the one that was approved when the mode
  was silently falling back to ring vertex 0.
* Whether the ring's shape changes with the pose (trap 5) is unanswered.
* The normal on cap vertices is inherited rather than forced on this asset,
  because NORMAL here is not a FLOAT3 and re-encoding a packed normal without
  knowing the asset's bias would be a guess.
