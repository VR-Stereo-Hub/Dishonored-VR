# Desktop presentation performance candidate

2026-09-14. [VR-115](https://linear.app/vr-stereo-hub/issue/VR-115), branch
`codex/vr-115-desktop-present`, based on accepted VR-Main `255d1c91`.
First implementation from the [VR-113 performance audit](https://github.com/VR-Stereo-Hub/Dishonored-VR/blob/codex/vr-113-performance-audit/docs/dishonored/PERFORMANCE_AUDIT.md).
Not installed or headset-tested. No game launch or merge.

## Purpose and scope

The accepted pipeline performs two original desktop Presents and a left snapshot
plus right restore per normal stereo pair. The audit's historical example windows
spent 2.7-3.9 ms inside the two native calls. These are CPU durations including
possible waits, not a promise of removable cost. Desktop delivery is the first
candidate because it has both measured cost and repeated work that the headset
does not need.

This branch adds two opt-in modes. Both leave the two native scene draws,
per-eye hooks, capture, image/pose tags, OpenXR work and engine hook count in place.
No engine memory writer or liveness cache changes. Shader caching and bridge
consolidation remain separate future improvements.

## Controls

| Mode | F10 Display | INI under VR | Live command |
|---|---|---|---|
| Full, accepted behavior | Both candidate checkboxes off | ReduceDesktopPresent=0, DesktopMirrorOff=0 | desktoppresent full |
| Reduced desktop updates | Reduce desktop presentation checked | ReduceDesktopPresent=1, DesktopMirrorOff=0 | desktoppresent reduced |
| Desktop updates off | Disable desktop mirror checked | DesktopMirrorOff=1, overrides reduction | desktoppresent off |

Both new keys default to 0, including absent-key fallback. F10 persists changes;
SAVE AS DEFAULTS also writes both keys. The command seam is a live A/B and does
not persist by itself. `desktoppresent status` reports the resolved values.
The two-key design permits returning from Off to a previously selected Reduced
mode. The reduced checkbox is disabled while Off overrides it.

Off freezes the last desktop image; it does not hide the window or turn off VR.
The existing `vrmirror off` means bypass the pin callback and is not this new
feature. If that callback is absent, these new modes fall back to native Present.

## Reduced mode

Only an identified current right draw immediately after a successfully displayed
left can omit its original desktop Present and the restore StretchRect. The
left snapshot remains, so unknown frames and fallbacks can use the accepted pin.
The permission is consumed even when it refuses; it cannot suppress consecutive
calls or survive a gap in hook serials. It does not use the delivered eye tag to
identify current D3D9 pixels, preserving the VR-76 distinction.

Required conditions include a fresh delivered capture serial, a produced texture,
live XR session, reentry projection, draw-source pin, valid left snapshot,
adjacent successful left native Present returning exactly D3D_OK, and standard
Present arguments. A positive status such as occlusion is not D3D_OK.

The swapchain must report windowed DISCARD, one backbuffer, no multisampling and
immediate presentation. Actual parameters are queried once per device/reset/pin
lifecycle, not inferred from an INI fullscreen request. Unsupported parameters,
custom rectangles/window/dirty-region arguments, copy failures, repeated or
unknown eyes, absent callbacks, missing output or stale capture use the native
path. Resize/reset/device/source transitions invalidate the permission.

## Off mode and command submission

With a fresh capture, live XR session, runtime mirror callback, standard arguments
and supported swapchain, Off omits all mirror copies and the native Present.
This includes captured mono UI frames. Missing fresh output or lost session falls
back to normal presentation, so the desktop may update during such transitions.
Old pin provenance is invalidated while Off; returning to Full/Reduced requires
a new left snapshot and cannot restore an old pre-Off view.

An event query issues END for the current D3D9 command stream, then makes one
GetData(D3DGETDATA_FLUSH) call. S_FALSE is pending, not completion. There is no
spin, sleep or CPU wait for GPU completion in this production path. The query
does not authorize shared-slot reuse or replace capture's ownership fences.
Query creation/issue/poll errors refuse Off until reset or an Off toggle, and
call the original Present. The query is released before device Reset.

The first implementation retained a pending query and polled it again next
frame. The native test disproved it: frame 2's independent GPU marker remained
pending for two seconds because an already-completed older event could return
without submitting newly queued work. The corrected implementation issues END
every frame, intentionally abandoning the old event result. This query only
requests submission; no consumer depends on its discarded result. Microsoft
documents END reissue in the issued state as abandoning the previous query.
[Query states](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries),
[GetData](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dquery9-getdata).

DISCARD does not preserve backbuffer contents after native Present. The host
model poisons discarded buffers and renders new content on each draw. The real
game's buffer/driver behavior still requires headset validation; a source model
cannot prove that every engine path honors this contract.
[D3DSWAPEFFECT](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dswapeffect).

## Timing and evidence

The runtime callback defers mirror work only when a candidate mode is selected.
The final decision happens after on_present_end returns, because a runtime
failure can occur after its mirror callback. In Full mode, mirror work keeps its
accepted callback position. In all modes, capture precedes any desktop action.
The runtime HUD hook is a no-op in this host; this deferral would need revisiting
if it ever wrote into the D3D9 backbuffer.

Every three seconds `desktoppresent:` reports hooks, actual calls, omitted calls,
Off omissions, non-OK native results, and categorized fallback counts. CPU costs
separate native Present, mirror work and Off submission. Means identify their
population: per hook versus per actual call. Native maximum is also recorded.
Per-present records retain action K (Reduced) or O (Off), nativeCalled and timings.
The original mirror shadow/copy counters do not count Off frames.

The older perf game-Present bracket now includes the host tail, including deferred
mirror work in candidate modes. Use the new native and mirror timings for
attribution; do not compare that one old column as if its scope were unchanged.
The hook/pair cadence still includes omitted desktop calls. None of these desktop
counters is a fresh OpenXR pair count or a GPU timer. Existing D3D9 span-label
issues and missing full D3D11 attribution remain VR-67 work.

## Validation

- `tools/desktop-eye-host.ps1`: accepted policy suite, legacy delayed-tag negative
  control, and production copy/tail module against a deterministic D3D9 device.
  79,339 policy and 431,857 copy/tail assertions pass. Exhaustive length-eight
  L/R/unknown sequences cover current and delayed tags. Includes failures,
  unsupported modes, custom arguments, missing callbacks, lifecycle and toggles.
- In that model, 100 normal pairs in Reduced retain 200 synthetic captures,
  perform 100 native Presents and 100 left snapshots, and perform zero restores.
  In Off, 100 synthetic captured frames perform zero native Presents or mirror
  copies and make 100 submission requests. These are operation counts, not FPS.
- `tools/desktop-present-d3d9-host.ps1`: standalone real 32-bit D3D9Ex HAL device,
  System32 D3D9, hidden 64x64 render target, no game/proxy/OpenXR. Queues an
  independent GPU event before the production Off tail and polls it without
  FLUSH afterward. 120 GPU completions and alternating pixel checks pass with
  zero native Presents; Full return and ResetEx pass. Test refuses to run while
  Dishonored is running. It does not model headset pacing or the engine renderer.
- Release build passes. Production default writer, repository release profile
  and golden INI are byte-identical, with only the two new default-off keys and
  their comments added to the accepted profile. Reentry ring and single-tag host
  regressions pass. Final exports/lint and artifact identity accompany the saved
  candidate. No installed settings were changed.

## Deferred test and continuation

The user is validating the entire opening on installed accepted build266 first.
Do not install this candidate or arm a diagnostic during that run. Installed
identity is still in `build/playtest-candidates/installed.json`; this branch's
artifact belongs in `build/playtest-candidates/vr-115-desktop-present/` and must
not overwrite that installed manifest. Check the log banner before interpreting
the opening, then preserve both logs before any later relaunch.

When installation is requested, first ask one question of a fixed, repeatable
scene: does Full -> Off -> Full improve complete stereo-pair frame times without
stale eyes, flicker, extra latency or texture faults? Keep resolution, scene,
refresh, capture, pose lag and diagnostics fixed, warm each segment, and repeat
baseline. Expected supporting evidence: Off omission count rises, native/copy
cost falls, and pair median/tails improve beyond repeated-baseline noise. If the
wait moves into capture/XR or pair times do not improve, reject the performance
hypothesis even if desktop-call count falls. Off's frozen window is expected.

Reduced is a separate subsequent A/B question for users who want a live desktop.
Then separately validate menus, notes, loading, cinematics, weapon/head movement,
return to Full, focus changes and device reset. One question per launch, with
outcomes defined before the tester starts. No FPS gain, headset correctness or
120 Hz result is claimed yet. No merge until explicitly authorized.
