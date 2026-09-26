# Next session: implement FSR on the depth foundation

Read CLAUDE.md fully, only the newest three sections of docs/STATUS.md, and this brief.
The maintained implementation plan is
[PERFORMANCE: FSR implementation](PERFORMANCE.md#2026-09-26-fsr-implementation-plan-and-depth-foundation-handoff).
Read that section rather than the entire performance history. Read the newest
FLICKER_REFERENCE entries and HANDOFF-GINGASVR Traps/Dead ends before runtime edits.

## Workspace and boundaries

- Work in `C:\dev\Dishonored-VR\build\worktrees\fsr`, branch `codex/fsr-implementation`.
- Parent: `claude/motion-vectors`, worktree `C:\dev\Dishonored-VR\build\worktrees\mv`.
  It remains unmerged and dedicated to the depth foundation, including existing TAA tests.
- Child starts from the parent's documentation handoff after runtime commit `1d2ee24a5`.
  Use `git merge-base HEAD claude/motion-vectors` to identify the exact handoff commit.
- Do not switch/reset the main checkout, rebase onto staging, merge either branch, rename
  the parent, delete branches, or remove working foundation code to make an FSR-only diff.
- No subagents. Credit commits to BioVRDev. No trailers or generated attribution.
- Search/verify a Linear ticket before using a number. FSR search was empty at handoff;
  prior sessions record an issue-capacity limit. No FSR ticket number exists yet.

## Current evidence and installed state

The tester reports no visible TAA benefit. Stop custom-TAA tuning as the next task. The
previous synthetic tests improved, but that is not headset quality acceptance. Exact live
A/B settings were not independently verified in the planning session.

The depth pipeline is proven for transport and coarse camera reprojection. Scene alpha
units and sky threshold remain approximate. c5 is negative world position, converted once
in clarity::view_for. Mirror test favoured normal axes. The 200 uu/unit scale is a coarse
minimum, not an exact engine derivation. Production TAA fuses reprojection; FSR needs explicit
vectors again. Deliberate jitter and animated-object vectors are missing.

Installed DLL SHA256:
`f25fc06e5a6d2f07d241cd071d84c4ea87b9f21b4e25372a8d289d8fed75d32b`.
Previous verified banner: `v1.0.1-93-gf0ef210dd-dirty`, Sep 26 2026 17:47:48.
Read-only INI check at handoff: Temporal=0, TemporalBlend=0.12, Sharpen=0.40,
MotionVectors=1, DepthShare=0, MotionCalib=0. Do not replace these with older restore values.
No installed files changed in the planning session. Read current state again before install.

Logs: `C:\Program Files (x86)\Steam\steamapps\common\Dishonored\Binaries\Win32\dishonored_vr.log`.
Archive current and previous logs with DLL/INI before installing/relaunching. Check banner
and DLL hash before interpreting a playtest. Compare the entire installed INI on every
install and preserve/verify CRLF. Leave DepthShare/MotionCalib off after tests.
The tester launches the game; prior simulator launch permission was for the motion-vector
task. Do not assume blanket permission for future FSR launches. Host tests do not launch it.
One clearly defined question per headset launch, with expected outcomes given beforehand.

## Start here

Target FSR 2.2.1 plus the AMD-published DX11 backend patch, in-process x86. This selection
fits the existing D3D11 interop architecture but is not yet proven to build/run in Win32.
First deliver a pinned/licensed dependency and an isolated x86 D3D11 host test with two
independent FSR contexts, synthetic inputs, deterministic output checks and allocation sizes.
Do not install an x64 SDK DLL or pull Unity into the runtime. Do not substitute FSR 1 or add
frame generation. If Win32 support fails, document the precise blocker and alternatives.

Then follow the plan's gates: fixed display output versus reduced render input; safe scene
projection jitter and paired eye phases; measured depth conversion and motion contracts;
colour/exposure/masks; lifecycle/F10; controlled performance and headset comparison. The
existing scene classifier is NOT safe to reuse blindly for jitter. Existing F10 total-pixel
resolution starts at 50%, while FSR Quality needs about 44.4% at the same output dimensions.
Native output and reduced input must be separately owned and verified.

Useful source map:
- `src/core/gfx/clarity.cpp`, `clarity_gpu.cpp`, `clarity_math.h`: current temporal reference.
- `src/core/gfx/depth_probe.cpp`, `capture.cpp`: serial association and GPU fences.
- `src/core/gfx/motion_gpu.cpp`: calibration and camera reprojection reference.
- `src/core/vr/pose_record.*`, `src/game/dishonored/scene_draw.cpp`: image/pose provenance.
- `src/core/gfx/reentry.cpp`, `src/core/vr/openxr_runtime.cpp`: eye output/submission.
- `src/core/ui/overlay_tabs.inc`, `src/core/config/config.cpp`: controls/default persistence.
- `tools/taa-audit-host.ps1`, `tools/motion-gpu-host.ps1`: 72 and 6 baseline GPU checks.
- Previous generated test/build evidence is only in the parent worktree's `build/taa-fixes/`.
  Do not expect gitignored binaries/evidence to be copied to this fresh worktree.

## Copyable starting prompt for Claude

Continue in C:\dev\Dishonored-VR\build\worktrees\fsr on codex/fsr-implementation.
Read CLAUDE.md, the newest three sections of docs/STATUS.md, then
 docs/dishonored/NEXT_SESSION.md and the "2026-09-26: FSR implementation plan and
 depth-foundation handoff" section of docs/dishonored/PERFORMANCE.md.

Implement that plan, starting with gate 1: pin FSR 2.2.1 and AMD's DX11 backend patch,
prove an in-process x86 build and a two-context synthetic GPU dispatch before touching
in-game projection jitter. Continue through the gated integration once prerequisites pass.
The existing custom TAA gave no perceptible improvement, so do not spend this session
retuning it. claude/motion-vectors is the unmerged depth foundation; this child branch is
for FSR. Do not merge either branch, rebase onto staging, or modify the main checkout.
Keep the current D3D9/D3D11/OpenXR architecture. No frame generation or FSR 1 substitution.

No subagents. Credit commits to BioVRDev. Build/install required candidates yourself;
I do not run commands. I launch the game. Explain one test question before each needed
headset launch, then read the matching log yourself. Preserve the entire installed INI,
verify CRLF and whole-file diffs, and leave DepthShare/MotionCalib off afterwards. Record
results and failed approaches in PERFORMANCE.md and keep this handoff current. Do not
claim a visual or speed improvement until it is actually demonstrated.
