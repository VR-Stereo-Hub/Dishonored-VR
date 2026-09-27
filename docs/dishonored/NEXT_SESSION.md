# Next session: projection jitter for DLSS (and later FSR 3.1)

Read CLAUDE.md, only the newest three sections of docs/STATUS.md, and this brief. The full
record is docs/dishonored/PERFORMANCE.md from "## 2026-09-26: DLAA through an x64 NGX helper"
to the end (grep the headings, read windows; the file is large). The older FSR plan section
"2026-09-26: FSR implementation plan" holds gate 3 (jitter) requirements that still apply.
Read the newest FLICKER_REFERENCE entries (the two DLAA smear entries at the top) before any
render change.

## Workspace and boundaries

- Worktree `C:\dev\Dishonored-VR\build\worktrees\fsr` (directory name kept), branch
  `claude/dlss-dlaa`, stacked on the unmerged `claude/motion-vectors`. Latest commit
  `afb72687a`. No merges authorized; do not rebase onto staging or touch the main checkout.
- Order agreed with the maintainer: DLAA (done, headset-accepted) -> DLSS SR (done, measured)
  -> projection jitter (this session) -> FSR 3.1 in-process.
- Commits credited to BioVRDev, no trailers, 2-10 per branch, no personal names anywhere
  (credit upstream projects by URL only). No Linear ticket exists (workspace issue limit).
- The maintainer launches the game for headset tests. Self-launching the simulator was
  permitted "for now" in the last session; ask again before launching in this one.
- NGX SDK: `tools\fetch-ngx.ps1` (pinned v310.7.0, gitignored, already fetched in this
  worktree). `tools\build.ps1 -Release` also builds the x64 helper; `tools\install.ps1 -Release`
  installs it to `<game>\dvr_dlss\`. `tools\dlss-host-test.ps1` = 13 host checks
  (`-Cost` adds the per-preset timing table).

## Installed state at handoff (read it again before installing anything)

- `d3d9.dll` SHA256 `c1a9268cd4abf5bcbfe6f9cd4f63afc027844762193584c28c4d9e1af398fb6a`
  (build 21:32, commit afb72687a + docs), helper `16cd5a5f...`, nvngx_dlss.dll 310.7.0.0.
- INI SHA256 `2e2183b71ebf498be529d720c0ad3c14836b1d108646b59900f2ad93f37efc05` - the
  maintainer's own settings since the last session: `DLAA=1`, `DlssModel=1` (fast), `DlssMask=1`
  (0.025..0.115), `DlssQuality=0`, `RenderWidth/Height=2750x2850`, Temporal=0, MotionVectors=1,
  DepthShare=0, MotionCalib=0. Launch file `-ResX=2750 -ResY=2850`. Preserve them.
- Back up DLL + whole INI + logs before every install (`build/dlss-install/pre-*`), restore the
  INI byte-for-byte after any simulator run (seam words and live resizes write it), check CRLF,
  and check `dishonored_vr_launch.txt` still matches `[Screen] RenderWidth/Height`.

## What exists (the pieces jitter plugs into)

- `src/core/gfx/dlss.cpp` (lifecycle, SR resolution transaction with the game side),
  `dlss_client.cpp` (x86 transport; `EyeInputs::jitterX/Y` already travel to the helper as
  render pixels, currently 0), `dlss_gpu.cpp` (guides: reversed depth, vectors through the
  game's own matrices, optional mask, `dlss audit on` = vector audit + flow check),
  `src/tools/dlss_host/dlss_host.cpp` (x64 NGX helper; passes InJitterOffsetX/Y).
- Vectors: `clarity.cpp` DLSS branch passes `vpCur/vpPrev` (the world view-projection captured
  at the c5 upload, stored per pose record at present by `pose::note_render_pos`) plus the c5
  change; the shader solves the pixel's camera-relative point and reprojects. Matrix layout
  MEASURED: camera-relative, row vector, `clip = [P - C, 1] * M`, clip.w = linear view depth
  in uu; depth units * 250 = uu (`MotionDepthScale`, flow-check fit). Arms nearer than
  `DlssBodyDepth` 0.30 get no translation.
- The flow check (`dlss audit on`, log `dlss/flow`) reports per image the whole-image shift
  and the scatter around it: it is the ready-made instrument for jitter (a jitter the vectors
  do not know about shows up as whole-image shift equal to the jitter).

## The task: deliberate sub-pixel projection jitter

Why: Super Resolution (and DLAA) reconstruct detail from sub-pixel sample positions; today only
head micro-motion supplies them, so SR is softer than it should be. DLSS expects the scene
rendered with a per-frame sub-pixel offset of the projection and that offset reported as
`InJitterOffsetX/Y` in render pixels.

Where: `src/core/framework/vs_const_hook.cpp`. The world view-projection is c0..c3 at the c5
upload (VR-65 comment there). An existing c0 patch (LeanVP, the positional lean on the vp lane)
already rewrites that matrix, gated by `IsMainScenePass()` in `vs_const.cpp`. **That classifier
is NOT safe for jitter**: it assumes a wide landscape target (aspect 1.4-2.4) while the eye
render is ~2750x2850 (aspect ~0.96), treats an unknown target as the scene, and c0..c3 is
re-uploaded per pass and per object (hundreds per view: shadows, reflections, UI). Identify the
world passes by observed target identity (the eye-size render target the capture reads, and
the scene RGBA16F depth target) and make a pass MOVE before trusting it (CLAUDE.md rules).

How (row-vector layout): NDC x shifts by `2*jx/W` when column 0 gains `(2*jx/W) * column 3`
(clip.x += k * clip.w); y likewise with `-2*jy/H` on column 1 (check the sign). Apply the same
offset to every draw of the world passes of one eye image, colour AND depth together, never to
shadow maps, reflections, HUD/Scaleform or our own passes. Record the applied offset with the
image (pose record, next to `renderVp`), so the pair (image, jitter) cannot come apart.
Remove it from the vectors: the stored VP includes the jitter, so either store the unjittered
matrix for the vectors or pass DLSS vectors that exclude it - decide by the flow check.

Sequence: Halton(2,3), 8-16 phases scaled to render pixels (DLSS guide: phase count ~ 8 *
ratio^2 for SR). Both eyes of one stereo pair use the same phase; advance per rendered pair,
not per present; reset on history resets. Jitter must not move the pose submitted to OpenXR.

Levers: default OFF with a live A/B (`dlss jitter on|off`, `[Clarity] DlssJitter=0`); refuse
and log when the world-pass identification is not confirmed.

Verification, in order:
1. Host test: extend `tools/dlss-host-tests.cpp` - render a slanted-edge scene shifted by a
   known sub-pixel jitter per frame, pass the jitter, assert DLSS SR output edge error drops
   against zero jitter, and that a flipped sign is worse (the sign convention, like the vector
   sign test).
2. Simulator with `dlss audit on`: with jitter on and the jitter NOT removed from the vectors,
   `dlss/flow` whole-image shift should equal the applied jitter's magnitude; removed, it
   should return to ~0.07-0.13 px. Also confirm shadow maps/HUD did not move (capture a shot,
   compare HUD positions).
3. Frame rate unchanged within noise; no new "pushed eye TWICE" outside resizes.
4. Headset question (one launch): does Quality SR with jitter look sharper than without, with
   no shimmer or crawling on edges and HUD?

## Measurements to carry (simulator, uncapped, per eye)

Native 2750x2850 133-153/s (varies by run). DLAA K ~70, DLAA fast 86, Quality SR fast 131,
Performance SR K 115. The game's GPU cost barely follows pixels (a quarter of the pixels saved
0.7 ms per eye), so SR does not buy frame rate here; jitter is a quality change. DLSS preset
costs per eye at 2750x2850 output: K ~2.0-2.2 ms, CNN E/F ~0.9, M 2.8-8, L 3.3-9.7.

## Open items (not this task)

- One stale eye per live resize ("pushed eye -1 TWICE" within 0.2 s of `res/live: CONFIRMED`),
  pre-existing; offered as a separate task.
- DLAA K is bound by GPU contention between the game and the helper process; one batched DLSS
  submission per frame (both eyes) would need a per-eye texture and deferred swapchain copies.
- FSR 3.1: the upscaler + DX12 back end build as Win32 static libs with three patches
  (PERFORMANCE, "FSR 3.1: the 32-bit build question"); scratch build in `C:\dev\fsr-src`.

## Copyable starting prompt

Continue in C:\dev\Dishonored-VR\build\worktrees\fsr on branch claude/dlss-dlaa. Read
CLAUDE.md, the newest three sections of docs/STATUS.md, docs/dishonored/NEXT_SESSION.md, and
the DLSS sections at the end of docs/dishonored/PERFORMANCE.md. Implement deliberate sub-pixel
projection jitter for DLSS as the brief describes: identify the world passes safely (not
IsMainScenePass), jitter colour and depth together per stereo pair with a Halton sequence,
record the offset with each image, keep it out of the motion vectors, pass it to DLSS, default
off with a live toggle. Prove the sign in the host test and the removal with the flow check
before asking for a headset run. Build and install yourself; back up and restore the INI
around simulator runs; ask before launching anything. Do not merge.
