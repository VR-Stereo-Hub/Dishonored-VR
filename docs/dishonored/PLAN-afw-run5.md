# PLAN: AFW run 5 - hands shrink and jitter, white edge dots (VR-39)

Status: REVIEWED (2026-09-28); step 1 built. Branch `claude/vr-39-aer-stereo`, build under test
`v1.0.1-164-g4b8e7a565` (the seeded two-source rebuild, `core/gfx/afw_warp.cpp`). Run-5 log archived in
`build/playtest-candidates/vr-39-aer/run5` (local only).

## 1. What the headset showed (run 5, 144 Hz, DLAA on, VirtualDesktopXR)

- **Hands and weapon jitter constantly**, whether or not anything moves. The tester sees a smaller copy of
  the hands and weapon, with a full-size copy jittering around its outside. Turning no longer changes it.
- **Near world objects jitter a little too**, the same way.
- **Thin light lines at the edges of some objects.** A fine-mesh catwalk grate shows its holes as a
  flickering array of white dots.
- **The flicker is worse without DLSS** (aliasing looks amplified). DLAA and DLSS Quality are much steadier.
- **Better than run 4:** no micro-stutter, and moving feels smooth.

## 2. Measured in the run-5 log

- **Every present was rebuilt from both sources.**
  - `afw/warp: beat ... full 330..427`, 0 NOT rebuilt, 0 depth misses.
  - The stale-capture, depth-ring and fallback faults of runs 4 and 5 are gone.
- **The game matrices were used for most presents,** but the basis check refused them in bursts:
  - 27 of 323 refused at worst 6.73 deg; 77 of 424 at worst 2.85 deg.
  - A refused present falls back to the XR pose + body yaw model, so while moving the held eye's world
    model switches between two models from present to present.
- **GPU cost of the rebuild:** 1.75-2.57 ms mean, 5.39 ms max, against 1.5 ms on the host.
  - DLAA also costs about 2 ms per eye image, in the helper.
- **Presents/s:** 109-142 on 144 Hz.

## 3. Reading the symptoms (hypotheses, each with what would refute it)

### A. The hands: a stereo scale error, shown every other present

Each eye alternates between two images of the hands:
- its own true render (when it is the fresh eye);
- the rebuild, whose hands come from the OTHER eye reprojected through depth (rule 1 of the compose:
  a fresh candidate nearer than the body threshold always wins).

If that reprojection is geometrically off, the eye sees the two hands swap at 72 Hz. That reads as a
wrong-sized copy with a jittering outline, and it does not depend on motion. That matches the report.
The run-4 temporal-only warp did not show it because it never reprojected across the eyes.

Candidates for the geometric error:

- **A1 - The foreground pass is projected differently from the world.** The arms and weapon are one
  `SDPG_Foreground` component (ENGINE_NOTES, "There is no separate arms mesh").
  - Mechanism: if the foreground is drawn with its own FOV or projection, the ray per pixel is wrong for
    those pixels.
  - Prediction: the reprojected hand is scaled about the image centre ("smaller"), the error grows with
    distance from the centre, and near WORLD objects do not share it.
  - Refuted if near world geometry shows the same error.
- **A2 - The foreground depth is not in the world's units.** A different or compressed depth written to
  the scene alpha for that pass.
  - Prediction: a horizontal-only disparity error proportional to 1/z, hands only.
  - Refuted if the error has a vertical component or shows on world geometry.
- **A3 - A global disparity gain error.** The metres per depth unit (`MotionDepthScale` 250 over the world
  scale 108) or the claimed tangent is wrong at near range. The 250 was fitted for motion vectors, where
  a gain error is mostly harmless, not for 63 mm stereo disparity.
  - Prediction: near world objects show the same horizontal error as the hands, scaling with 1/z.
  - That matches the second report ("close objects jitter a bit"), so A3 is the leading candidate. A1 or
    A2 may add to it.
- **A4 - The hands are drawn with a different eye offset from the world.** For example the viewmodel at
  the head centre for both eyes.
  - Prediction: a constant horizontal offset of about IPD/2/z for foreground pixels only.

### B. The white dots and edge lines: the source switch at thin structures

- **B1 - The disocclusion fill reaches too far.** `fill()` takes the FARTHEST covered seed up to 128 px
  along the row. Behind a grate, the farthest thing is often a bright, far surface.
  - Every hole the two eyes see differently is a small disocclusion, which fits an array of white dots.
  - Prediction: the red debug tint (last resort) is on the dots.
- **B2 - The source switches at edges from present to present.** At a thin bar each pixel picks the
  held or the fresh candidate separately, and the pick can flip from one present to the next.
  - The inputs are aliased without DLSS, and even with DLSS the scene depth is jittered while the colour
    is not.
  - Prediction: worse without DLSS (reported); the tint along edges flips colour between presents.
- **B3 - The depth is jittered and the colour is not.** Under DLSS/DLAA the depth the warp uses is drawn
  with the projection jitter (`Record.jitter`); the colour is the resolved, unjittered image.
  - Depth and colour edges then disagree by up to about a pixel, differently every present.
  - Prediction: edge flicker that follows the jitter phase; offsetting depth reads by the recorded
    jitter removes it.
- **B4 - Linear colour sampling across a depth break.** A sample that lands within a pixel of an edge
  blends foreground and background colour into a light seam.
  - Prediction: a thin line along object silhouettes, steady rather than flickering.
- **B5 - The seed maps are at half resolution with a grid step of 2.** Grate bars thinner than about 2 px
  fall between seeds.
  - Prediction: bars thinner than 2 px lose their seed, and their pixels take the refine-from-t path.

### C. The world model switching while moving

- The basis-check refusals (above) swap the held eye's world between the matrix model and the XR pose
  model on single presents. The two differ by walking parallax, so the switch is a small jump.
- Why the rotator and the matrix disagree by up to 6.7 degrees on some presents is not known.
- Candidates:
  - The rotator is the camera the seam WROTE, and the engine adjusts the view after that write.
  - A camera shake or view-bob component.

## 4. The adversarial review of this plan (2026-09-28), and what it changed

The review checked the plan against HEAD `4b8e7a565` and the run-5 log. Every finding below was
accepted.

- **The world-model hysteresis would weaken its own checks.** Three refusals before switching lets a
  record that alternates bad and good stay in matrix mode for ever. A refused record must fall back at
  once. The refusals are diagnosed before any policy changes: the log now names the axis (forward,
  right and up), both records' ids, the writer and the write-to-capture time.
- **"Held first" as written would put a world-moved hand on screen.** The held candidate carries
  body-yaw and walking motion; hands move with the body, not the world. A held-first policy for hands
  needs its own held-body candidate, validated, and a fresh-source escape. The claim "correct whatever
  the calibration" is withdrawn.
- **The fit cannot classify A1, A2 and A4.**
  - For parallel eyes, disparity is f B / Z. A foreground FOV change, a baseline change and a depth
    scale error all produce the same disparity-gain error; a different foreground FOV need not produce
    a radial scale.
  - Depth bands do not identify the foreground: a 0.5 m wall falls under the body threshold, and a
    weapon beyond 0.93 m falls above it.
  - It is an EFFECTIVE disparity fit, diagnostic only.
- **Head stillness does not make the two images comparable.** They are from different instants.
  Hands, weapon animation and shading move, and textureless patches fit anything. A live automatic gain
  is out of the first build.
- **Class history compares different scene points.** Successive rebuilds of an eye have different
  target poses, so an unwarped class map is not an incumbent source. Pinning is out.
- **The proposed temporal metric passes the fault itself.** A consistently wrong rebuild alternating
  with a correct native frame changes nothing between rebuilds. The metric must be the native/rebuilt
  sequence of both eyes, inside the feature, without the outline exclusion.
- **Full-resolution seed targets cannot recover source geometry the grid never sampled.** A 1 px bar
  between grid columns is missing from both seeds. Source sampling and destination resolution are
  separate questions.
- **Jitter is NOT a run-5 cause.** The run's log has `DlssJitter=0`, and projection jitter was off in
  all 55 DLSS status lines. For later, the convention is in `dlss_jitter.h`: raw pixel p samples scene
  point p + j. Raw depth for an unjittered u is read at u - j (in render/depth uv); a seed from raw
  depth d sits at d + j.
- **The fill has no layer definition.** "Nearest background" is ambiguous with several layers; the red
  tint was shared with the held fallback. The fill now has its own tint (magenta).
- **Cost.** Per-pixel atomics, extra reads and full-resolution seeds all add up. Diagnostics must be
  sparse and budgeted; tail latency and missed deadlines count, not only the mean.
- **The rollout broke the project's rules.** It changed several render behaviours at once, defaulted
  them on, and asked several headset questions in one run. The corrected order is below.

## 5. The corrected order

### Step 1 - evidence (this build: `v1.0.1-165`, no rendering change)

1. **`afw dump [n]` and the F10 button** "Capture AFW frames for diagnosis". Starting 5 s after the
   press, 16 consecutive presents are written to the data dir's `dumps\`. Per present:
   - the native fresh eye's colour and depth;
   - the held eye's colour and depth;
   - the rebuilt held eye;
   - both records: poses, targets, the matrix, c5, the rotator, the pose-record id, the writer, the
     write and capture times, the DLSS jitter and the body yaw.
   Game output, local only, never committed. The host test checks the files and their sizes.
2. **The basis-refusal detail line**, as above.
3. **The fill's own debug tint.**

**One headset question:** capture twice, both with the hands held still in view:
- once looking at the grate with DLAA;
- once with the upscaler off.

Nothing else changes, so the capture shows the fault as reported.

### Step 2 - offline classification (no headset)

For each consecutive pair, compare the native image of an eye with its rebuild one present later, and
with its native image two presents later. Measure, inside the hand's silhouette and along grate holes:
- the displacement field between native and rebuilt (horizontal only, radial, constant, or 1/Z);
- whether the fresh depth under the hand is consistent with the hand's stereo disparity between the
  two native eyes (block matching on textured patches, with a confidence and uniqueness threshold);
- which compose class produced each grate dot, by re-running the production shader on the dumped
  inputs on the host (the dump carries everything the rebuild read).

The answer names the error as a disparity gain, an eye offset, a projection difference, a depth
encoding of the foreground, or the fill. Each has its own fix.

### Step 3 - one correction at a time, default off, host-tested first

The correction that the evidence supports is added behind its own word, default off. It gets a
host case that reproduces the measured fault from the dumped inputs and fails without the
correction. It is then armed for exactly one headset question. The same then applies, one at a time,
to the grate or fill correction and, if the refusal cause needs one, to the world-model change.

## 6. What this plan does not change

- The seeded two-source design, the freshness and epoch guards, the matrix checks (a refusal still
  falls back at once), the depth snapshot.
- Pacing.

## 7. Step 2 result (2026-09-28): classified from the run-6 captures

- **A1 CONFIRMED** in its FOV form: the foreground is projected at the camera FOV (108.07), the world at
  `ProjectionFov` (103).
  - A pure depth gain (A3) is refuted: world surfaces from 0.93 to 70 m match within about half a pixel,
    and the near ratio equals the FOV ratio at every near depth.
  - A2 and A4 are not needed to explain the data.
  - The review's point stands: disparity alone could not have separated A1 from A2/A4. The FOV numbers
    in the log did.
- **B1 CONFIRMED** for the grate slats, by the replay tint: the fill.
  - B3 (jitter) is ruled out for this run.
  - B2 is not dominant: the held-eye dots appear in proportion to its share of the image.
- **Step 3 as executed:**
  - two corrections, each with a word and an F10 checkbox, each with a host case and a failing control,
    each replay-verified on the real captures;
  - the basis gate widened to 10 deg on the measured cause (rotator lag), not hysteresis.
  - Both corrections are on by default inside AFW, which is itself opt-in. That departs from "one
    default-off correction per run": they address two different surfaces, the replay isolates each, and
    the checkboxes A/B each live.
