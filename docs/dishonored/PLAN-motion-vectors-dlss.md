> Current direction (2026-09-26): the tester reports no perceptible custom-TAA benefit.
> Keep this branch unmerged as the depth foundation. The next child branch implements FSR;
> DLSS is not the next task. See PERFORMANCE.md, "FSR implementation plan and depth-foundation
> handoff", and NEXT_SESSION.md. The original plan and measurements below are historical.


# Motion vectors, then DLSS (plan, 2026-09-26)

Branch `claude/motion-vectors` off `staging` (`fb73099aa`). Depth transport and optional camera-vector TAA are built; headset A/B is next.
Why: the clarity work (`PERFORMANCE.md`, Anti-aliasing and clarity) showed that a temporal pass
reprojected by head rotation alone smears while walking, and only stays sharp by giving the
history up whenever the camera moves. Motion vectors fix that, and DLSS cannot run without them.

## What a motion vector is here

For each pixel of an eye image: where that surface was in the previous image of the same eye.
Two parts:

1. **Camera motion.** From the pixel's depth, the current camera and the previous camera, the
   world point is reconstructed and projected into the previous view. Exact for everything that
   is still in the world (walls, floors, props at rest) whatever the head and body do: turning,
   walking, crouching, Blink. This is most of the image and all of the walking smear.
2. **Object motion.** Anything moving on its own (NPCs, the hands and weapons, doors, particles)
   needs its previous transform. The engine does not keep it; producing it means drawing those
   objects again with last frame's matrices. Not planned; the temporal clip handles them as it
   does now.

## What the mod already has

- Both cameras exactly: the pose record per eye image (`core/vr/pose_record`) carries the written
  rotator and position; the projection is the layer's claim.
- A per-eye history and a reprojecting pass (`core/gfx/clarity`), host-tested on the GPU.
- A frame dumper (`dump` on the seam) and the device census (every render target the game makes).

## What is missing: the depth

The game has no depth-texture path on D3D9 that the census has seen: its depth-stencils are
plain D32 surfaces (not sampleable), and the exe carries no INTZ or RAWZ format strings (one DF24
reference, most likely the shadow path). UE3 on D3D9 of this era keeps scene depth in the ALPHA
of its floating-point scene colour target for its own post effects (the `SceneDepth` shader
parameters in the exe). So the plan starts by proving where depth lives:

1. **Probe (next build).** Find the scene colour target by its identity (float format, eye size,
   bound for the opaque pass) and read its alpha at a few pixels whose distance is known (the
   hand at arm's length, a wall at a measured range from the c5 camera). A depth that does not
   move when the probe points move, or does not match the known range, fails the hypothesis.
   Log the verdict; no image changes.
2. **Copy.** Once identified, share that target (or a depth-only copy of it) to D3D11 per eye,
   the same way the colour capture does, before post-processing overwrites or tonemaps it.
3. **Reconstruct.** A D3D11 pass writes a motion-vector texture per eye from depth + the two
   cameras. Host-tested like the clarity passes: a synthetic scene with known depth, a known
   camera move, the vectors checked to a pixel.
4. **Use it.** The temporal pass switches from rotation-only to depth reprojection, keeps its
   history while walking, and the motion weighting is retired. A headset A/B decides.

## DLSS after that, and its blockers

- **32-bit.** The game and the mod are 32-bit; NVIDIA ships DLSS (the Streamline and NGX
  libraries) for 64-bit only. DLSS would run in a separate 64-bit helper process that shares the
  colour, depth and motion textures with the mod (D3D11 shared handles work across processes)
  and hands back the upscaled image. Latency: one extra cross-process fence per eye.
- **Jitter.** DLSS needs the render to move by a sub-pixel offset each frame, and to know it.
  The mod writes the camera, so a sub-pixel rotation jitter is possible; its effect on the
  projection must be measured, not assumed (a rotation is not exactly a screen shift at the edge).
- **Inputs it wants:** colour before the HUD and post effects, depth, motion vectors, exposure,
  and a reset flag on cuts. The HUD is already separate (VR-117); post effects are baked into the
  captured colour, which DLSS tolerates less well.
- **The win:** render below 100% and reconstruct to the headset size, which is the only way to
  get 200-300% clarity at today's frame cost. Licence: the DLSS SDK's terms must be read before
  anything is shipped.

## Order

Probe (1) is a single, small, measurable build. Nothing past it is worth building until it says
where the depth is.

## Result of step 1 (2026-09-26, simulator, build 15:34)

**Scene depth is in the ALPHA of the eye-size A16B16G16R16F render target (D3DFMT 113), read at
Present.** The game creates two such targets at 2750x2850 (probe candidates #6 and #7; identical
reads, most likely the ping-pong pair of scene colour). The other float targets are not it:
the half-size (1375x1425) RGBA16F targets read all zero at Present, the eye-size R16F and G16R16F
targets read zero with a single bright texel.

Evidence (5x5 grid, 10 %..90 % of each axis, alpha):
- Main-menu scene: top row about 4400 (sky), middle 2..14, bottom row about 0.8.
- In a room, head level: the top row CONSTANT across all five columns (0.2397, a ceiling: a
  plane parallel to the view's horizontal axis has one depth per row), the outer columns constant
  down the image (0.27 and 0.31, the side walls), the bottom row near-constant (0.67, the floor),
  1.1..3.4 inside. That structure is what linear view depth of a box room looks like and nothing
  else in a colour target does.
- The falsifiable prediction: head pitched 60 deg down with nothing else changed, the centre falls
  (1.17 -> 0.81, the floor along the line of sight) and the ceiling row stops being constant.
  Both happened.

Units: about 100 uu per unit (eye height over the floor at 60 deg down, ~80-90 uu, read 0.81).
Calibrate in step 2 against a measured distance before anything reprojects with it.

Next: step 2 shares this target's depth to D3D11 per eye image, at Present, beside the colour.

## Result of step 2 (2026-09-26, simulator, build 16:04)

The scene target (the first-created eye-size RGBA16F) is copied every present by StretchRect
into a D3D9 render-target texture opened on the mod's D3D11 device (`interop::create`, the same
route as the colour capture), fenced by an event query. `[Diagnostics] DepthShare`, off by
default; `depthprobe share on|off`.

Verification: every 5 s the SAME present is read on D3D9 from the game's own target and on
D3D11 from the shared copy, 25 texels each. 13 checks of 13 were bit-identical (worst difference
0) across the main-menu scene, a load and gameplay in a room; about 5200 copies, no refusal.
Simulator paced at 90 Hz, GPU per tick 7.9-8.0 ms with the copy on (a headset A/B still owes the
copy's cost; it is one full-size RGBA16F blit per present, about 62 MB at 2750x2850).

Next (step 3): pair each shared depth with its eye image and pose record (the colour capture's
tag and record, delivered on the same present), and write the motion-vector pass: world point
from depth and the current camera, projected into the previous camera of the same eye.
Host-test it against a synthetic scene with known depth and a known camera move, then feed the
temporal pass. The depth scale (~100 uu per unit) must be calibrated first: a surface at a
measured distance from the c5 camera.

## Step 3 prior handoff (2026-09-26, simulator) - completed below

Built: the depth ring keyed by the colour grab's serial (`depth_srv_for`), the calibration shader
(`core/gfx/motion_gpu`, host test `tools/motion-gpu-host.ps1`: 6/6, a sharp minimum at the true
scale, rotation-only 10x worse, flat with no translation) and the live instrument in
`core/gfx/clarity.cpp` (`calib_frame`, `[Diagnostics] MotionCalib=1` with `DepthShare=1`).

Measured on the simulator (head stepped 0.3 m sideways, about 150 moving frame pairs a run):
- With the translation as computed, every candidate scale (25..7000 uu per depth unit) scores
  WORSE than rotation-only, falling monotonically toward it: the predicted parallax hurts.
- SIGN TEST: the same pairs with the translation reversed give an interior minimum at 200-400
  uu per unit (0.0145 against rotation-only 0.0147). So a convention is flipped: either the
  translation's direction, or the image is mirrored left-right against the camera's right axis
  (a sideways move cannot tell the two apart).
- NEXT (built, installed, NOT yet run): the MIRROR TEST - pure head turns scored rotation-only
  with the normal convention and with the right axis mirrored; the lower error names the real
  convention. Run: sim launch, `boot.ps1 -Attach`, Space x3 to gameplay, then
  `xrsim-cmd "head rot 3 0 0"`, `"head rot -3 0 0"` repeated; read `MIRROR TEST` lines.
  If mirrored wins, the clarity temporal pass has the same fault (its yaw reprojection) - that
  may be the smear reported while moving before the motion weighting.
- Then: fix the convention in `calib_frame` (and clarity's temporal), rerun; the curve must have
  an interior minimum below rotation-only. Only then compute motion vectors for TAA.

## Steps 3 and 4 built (2026-09-26) - earlier candidate, superseded below

Mirror result: 56 pure turns, normal error 0.0082 vs mirrored 0.0468. Do NOT flip clarity's
yaw. `Cam::pos` comes from `last_written_pos`, which returns c5 = negative world position;
its old header was wrong. Clarity now converts once in `view_for`, so both calibration and
TAA use world coordinates without changing shared pose records or any engine-memory writer.

Corrected translation: 70 moving pairs, minimum at 200 uu/depth-unit (0.0226), rotation-only
0.0400, 65/70 votes for 200. This passes the reconstruction gate, but the scale is coarse.
Complete error curve, source provenance, failed tests and GPU/memory limits: PERFORMANCE.md,
"Motion-vector calibration and TAA candidate". Keep further research there.

Built: per-eye RGBA16F motion textures at TAA output size, xy previous UV minus current UV,
z validity. Depth and colour pair by capture serial. TAA consumes these vectors, retaining
colour clipping. Missing depth retains rotation plus motion weighting; invalid depth rejects
history; sky uses rotation. Active vector TAA requests depth independently of diagnostics.
The first no-diagnostics test caught an old caller gate (all fallback); fixed and rerun.
54 clarity GPU checks and 6 calibration GPU checks pass. The synthetic TAA test scores
0.00085 with vectors, 0.09700 rotation-only, 0.13496 with deliberately wrong translation.

Final installed DLL SHA256 `5469cd53f7b674c9247a9047f11be736d4db2d36355358929349448194ceb661`,
banner `v1.0.1-91-g35629a116-dirty`, built 16:54:27. Gameplay translation + turns complete
~450 vector-TAA passes per eye per 5 seconds, zero fallback; off/on recreates the depth ring
and resumes. DepthShare=0 and MotionCalib=0 throughout this final run. Simulator stopped;
full original installed INI restored byte-for-byte, CRLF verified. No merge.

Next headset question: with Temporal AA enabled, does enabling "Depth motion vectors
(experimental)" reduce walking/leaning trails while keeping edges stable? Toggle at F10 >
Advanced > Display > Clarity and anti-aliasing, then close the panel for the comparison.
Better supports the camera-parallax fix; unchanged/worse means scale, disocclusion or
object motion still limits it. Not yet headset-confirmed. Both levers default OFF.
`MotionDepthScale=200` remains an experimental coarse calibration, adjustable through the
seam; moving-object vectors, depth-history rejection and DLSS are not built.

Simulator recipe: launcher and boot as above; the boot harness's old menu-closed fallback
can report GAMEPLAY before a save loads. This session needed Enter after the first Space x3,
then another Space x3. Verify the latest state transitions actually reach loaded GAMEPLAY
before collecting head-motion pairs. Keep commands together in one game-cmd invocation,
or await their log acknowledgement: separate writes can overwrite the 1 Hz command seam.


## TAA audit fixes (2026-09-26) - RESUME HERE

Production TAA now reconstructs camera motion inside its temporal shader, without vector
textures. Optional materialized vectors remain for host geometry tests/future consumers.
History alpha stores depth for visibility rejection. Stationary detail is preserved, and
large colour changes at stationary camera pixels reject stale history. Per-eye draw c5 and
scoped FOV are stamped separately from tracking publication, with identity/scene-age guards.
Depth sharing has independent read fences and duplicate-serial invalidation; capture refuses
unsafe timeout delivery. Previous sections describe earlier candidates, not the current path.

72 host TAA checks, 6 calibration checks, frame tests and simulator transition recovery pass.
Tested/installed SHA256 `f25fc06e5a6d2f07d241cd071d84c4ea87b9f21b4e25372a8d289d8fed75d32b`,
Sep 26 17:47:48. Full original INI restored, diagnostics off. No merge. All research, failed
threshold experiment, timing data and limitations are in PERFORMANCE.md, TAA audit fixes.
Next: headset fine-detail/walking A/B. Jitter needs reliable projection-pass ownership before
implementation; animated-object vectors and exact depth calibration remain open research.

## Object motion (VR-39, 2026-09-29) - host-verified, headset pending

**Reported:** under DLSS, moving characters and the view from a moving vehicle smear. The vectors are the
camera's only: right for the static world, wrong for anything moving on its own (characters) or with the
camera (a boat). The game draws no velocity buffer (`MotionBlur=False`).

**Built: `dlss objmotion on|off`, `[Clarity] DlssObjectMotion` (default 0), F10 "Follow moving characters
and vehicles".** Each eye image is block-matched against that eye's previous image (`GuideGpu::objmotion`,
after the camera vectors, before the mask and the audit).
- `cs_objpre`, one thread per 8x8 tile:
  - the contrast test;
  - the camera match;
  - an exact early exit: a tile needs the camera SAD minus the best to exceed the minimum gain, so a camera
    match within it can never lose. The static world exits here.
  - Tiles left go on an append list.
- `cs_objsearch`, one 64-thread group per listed tile (an indirect dispatch):
  - the candidates: camera, zero (riding along), rotation-only (a turning vehicle), and last frame's tile
    and its four neighbours;
  - a 9x9 two-pixel coarse grid around the best and around zero;
  - two rounds of +-1, then a parabola for sub-pixel.
  - It wins only at best < 0.5 x camera AND camera - best > 0.02.
- `ps_objfix`, per pixel: the camera vector, or a winning neighbour tile's, whichever matches a 3x3 patch
  best. The camera is weighted x0.7, so the static world keeps its exact vectors.
- Jitter: the previous image is sampled at + (jitter - prevJitter), the flow check's convention.

**Host (`tools\dlss-objmotion-host.ps1`, 10/10), a 2.5 deg yaw with a textured panorama:**

| Case | Result |
|---|---|
| Static world | 0.000 px change |
| Character, integer motion | 0.00 px |
| Character, sub-pixel (2.4, -5.7) | mean 0.14 px, p95 0.25 px |
| Boat riding along | 0.00 px |
| Flat patch | camera kept |
| Temporal candidates | kept |
| Control (camera vectors alone) | character 16.4 px, boat 13.6 px wrong |

**Cost at 2114x2192** (per eye image, camera vectors subtracted, 20 frames between timestamps after a
warm-up): 0.25 ms with a still camera and one 400x500 character; 2.05 ms when every tile disagrees.

**Measurement traps paid for:**
- A single spaced-out pass reads the GPU's idle clock (the same work measured 8 ms, then 1.5 ms).
- Repeating the pass without the camera pass in between finds the vectors already corrected.
- A one-thread-per-tile search is latency-bound (1.7 ms).
- A per-pixel loop with dynamic array indexing spills out of registers.
- fxc refuses a barrier inside data-dependent flow (X3663), hence the list and the indirect dispatch.

**Open:**
- Untextured surfaces keep the camera vector. There is nothing to match, and DLSS cannot smear what has
  no detail.
- A character's first frame after it starts moving uses no temporal candidate.
- Headset A/B: F10 box on/off with a walking NPC in view and on the boat.
