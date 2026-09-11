# VR-76: implementation and validation handoff for Claude

2026-09-11. The mirror fix is implemented, built and installed for a user-run
headset test. It is a candidate fix, not a perceptually verified resolution.
The working branch is `claude/vr-76-hand-flicker-right`, based on `18d39cee`.
Changes from this session are in the working tree; no merge or ticket closure.

## Installed state

- DLL: `C:\Program Files (x86)\Steam\steamapps\common\Dishonored\Binaries\Win32\d3d9.dll`.
- Build ID: `vr33-hands-working-95-g18d39cee-dirty`.
- DLL SHA256: `264038D60BFBF6098328B8A4F034DD9BFF5EF4B9A6B0061917EB61FEFE1BC489`.
- Installed ini SHA256: `4C6B370F28A0FEC790B3B48AC5E39DA4309189430D0333031974831249EF9707`.
- The ONLY ini change is `[VR] DesktopEyeSource=draw`. Removing that added line
  reproduces the original ini text exactly. Capture, pacing, hand placement,
  motion aim, stereo method and resolution settings were preserved.
- Fresh-install/tree default remains `DesktopEyeSource=tag`, keeping the new
  lever off until there is a headset verdict. The installed test opts into draw.
- Original DLL, ini and both logs are preserved under
  `build/vr76-validation/before/`. Original DLL SHA256:
  `AA6255FFDB3D56FA9C486C3E172004E75F8D25F020AE2E55390461EA8EFEACCF`.
  Original ini SHA256:
  `FF600F74A76CC1EC2C9E011BCAE47D1BA19A8886883CEF9F9BD266C15B87006A`.

The user owns all further game launches and testing. One simulator game launch
was attempted before that constraint arrived. It reached mod configuration and
created an instance on dvr-xrsim, but no successful game simulation is claimed.
The launch harness was cancelled, no game process remained, and the original
DLL, ini and both logs were restored and hash-verified. The candidate DLL and
single ini addition above were then installed at the user's request, without
launching. The attempted startup log is `build/vr76-validation/cancelled-launch.log`.

## Review of the proposed mechanism

The capture/backbuffer identity mismatch is real in the code. The proposed
mirror mechanism is sufficiently supported to implement and test; it does not
prove that all reported headset symptoms have the same cause.

1. **F3 needs the SharedWait qualification.** Sync delivers the current pixels;
   deferred and shared with `SharedWait=0` deliver the previous slot. Shared
   with `SharedWait=1` delivers the current slot. The run explicitly logged
   `SharedWait=0` and previous-present delivery, so the qualification does not
   weaken this run's diagnosis. The current draw count k becomes Present k+1;
   game_tick precedes end_frame, so its tag observation aligns at k+2 for
   current delivery and k+3 for one-present delivery. Startup/mode switches or
   capture failure can instead produce no delivery; the formula is not universal.
2. **The mirror receives the resolved current eye before capture.** Reentry
   publishes `eye` after the c5 override and realignment logic and immediately
   before `set_pending_tag(eye)`. The realignment drain consumes future ring
   entries but explicitly retains `t.eye = inv` for the current sample. The
   capture reads the current D3D9 backbuffer. This identity remains the method's
   best available classification, not an independent pixel measurement. Early
   method failures and mono start with 0 from `begin_present`, preventing reuse
   of an earlier present's eye.
3. **The two-line runtime proposal misses two guards.** `mirror_present` itself
   rejected 0 before invoking the hook. Also, a successful pairHold returns
   after capturing the first eye, before the proposed unconditional tail call.
   Both are covered now. There are three mutually exclusive post-capture paths:
   no XR frame open, first eye held open, and normal completion. Each calls the
   hook once, including a delivered sign of 0.
4. **The proposed HUD tradeoff does not exist in this tree.** `composite_hud()`
   is a no-op. The intervening capture/frame-id work operates on the handed-in
   D3D11 texture and swapchains, not the D3D9 backbuffer. The runtime hook was
   retained before composite_hud on all three paths to preserve the seam if
   desktop HUD composition is implemented later. The runtime changes are marked
   `41.2 (Dishonored, VR-76)` and leave capture/submission logic alone.
5. **Unknown-eye hold needs a lifetime boundary.** Draw mode holds up to three
   consecutive 0-eye presents; the fourth invalidates the held image and lets
   menus/loads through. Repeated tagged right eyes reuse the left snapshot.
   A right-first startup, reset, resize, device change, failed snapshot, expired
   menu hold or source toggle has no guaranteed left pixels until a successful
   left snapshot. It passes the raw frame and logs it rather than reading an
   uninitialized surface. Thus the original unconditional zero-leak assertion
   must be scoped to a valid pin; warmup and graphics failures are explicit.
6. **The timing numbers reproduce; the significance claim does not follow.**
   An independent PowerShell pass over the original log found 38 markers,
   37/37 post-flicker markers within 600 ms of a single-tick transition line,
   delays 15/328/578 ms (min/median/max), and 458/1095 baseline sample windows.
   All 38 alignment summaries selected +3, including the baseline marker.
   The clustered markers and overlapping 600 ms windows are not independent
   Bernoulli trials, so `0.42^37` is not a defensible p-value. The correlation
   remains useful evidence. The roughly 15,800 placement rows are overlapping
   histories, not necessarily distinct presents. Their clean fields do not
   independently clear every later draw, transform or perceptual effect.

### Another diagnostic with the same identity problem

The `reentry: pair` / `pair geom` diagnostic gates on `delivered` but reads the
current `camera::render_pos(c5)`. In a delayed stream those c5 samples do not name
its delivered pixels. This is a logging issue, not a mirror or headset writer;
it was left unchanged to keep this patch scoped. Do not use its SWAPPED wording
as independent proof about the submitted eyes under delayed capture. The actual
hand overlay consumes the delivered texture with `delivered_tag()`, which is the
correct pairing. The marker's original tag history also keeps its measured join.

## What changed

| Area | Implementation |
|---|---|
| `desktop_eye_policy.h` | Pure shared production/test decision code, Tag/Draw source, None/Snapshot/Blit action, held-pixel validity, bounded unknown hold. |
| `desktop_eye.cpp/.h` | Current-eye publication, successful-copy provenance, source switch, counters and 1024-present history. DEFAULT-pool surface invalidated on reset, resize, device change, disable and source toggle. |
| `frame_hooks.cpp`, `reentry.cpp` | Initialize identity to 0 before every method call; publish resolved reentry eye before capture. No changes to tags delivered to XR. |
| `openxr_runtime.cpp` | Permit zero tags to reach the hook; remove early left-only call; call after capture on pairHold and normal exit. Existing no-frame path remains. |
| Configuration and command seam | Load/log `[VR] DesktopEyeSource=tag|draw`, including origin; persist via Save As Defaults; live `desktopeye draw|tag|on|off|status`. Invalid sources retain the existing value and warn. Runtime `vrmirror` gate is still separate. |
| `scene_draw.cpp` | Atomic count of actual single gameplay ticks where the 0 tag is pushed. This does not alter a gate or draw. |
| `mesh_split.cpp` | Append `desk=source:draw/tag/action/shown` to V-marker numeric records. Direct join to Present `drawCount+1`, independent of the delayed-tag alignment search. |
| Host harness | Policy sequences plus deterministic device-double tests that compile the actual copy module. No game or headset required. |
| Docs/defaults | Updated mirror reference, trap, architecture decision, marker fields, roadmap/status and default ini golden. |

A related existing defect was corrected: `g_snaps` was a lifetime counter, but
its nonzero value allowed a newly recreated surface to be blitted before its
first successful snapshot. Validity now belongs to the current surface. Failed
snapshot copies invalidate it; failed re-blits retain the source for a retry and
record the failure. This is included because safe pinning across reset/toggle
boundaries depends on it.

## Instrumentation and limits

The 15-second `desktopeye:` line now resets its window counters at the printed
boundary. Previously its purported window totals accumulated until 100,000
snapshots. New counters are:

- `draw L/R/0` and `tag same/opposite/zero`: zero means either side is unknown.
  Expect mostly opposite for steady shared/SharedWait=0 and mostly same for
  sync or shared/SharedWait=1. Repeated eyes and boundaries affect that ratio.
- `singleTicks`: actual single gameplay tick events on the game lane during the
  time window. It is a population check, not an exact present-number join.
- `oldShadowChanges`: known-eye display transitions under the legacy policy;
  startup, repeated eyes and boundaries can contribute as well as bursts.
- `oldShadowRawLeaks`: legacy policy does nothing while a known current eye
  differs from its known held eye. This sees the predicted left-frame leak
  after a single untagged draw in delayed capture. Shadow copies assume success.
- `shownR`, `shownUnknown`, `warmupR`, `failures`, `snap/blit`: actual selected
  actions and copy-result provenance. `shownR` includes startup/warmup right
  frames, which `warmupR` identifies separately. A failed copy is not a pass.

These are metadata/copy-outcome diagnostics, not a GPU pixel verifier. A 0-eye
frame has unknown provenance even if today's single-draw camera is left. An F
record means the shown-eye inference cannot establish correct final pixels.
The user's view of the desktop and headset remains the acceptance evidence.

Marker suffix examples: `desk=d:L/0/S/L` means draw source, current left,
no delivered eye, successful snapshot, inferred left shown. `desk=d:0/R/B/L`
means an unknown current draw was replaced by held left pixels. Source is d/t;
action is S/B/N/F for snapshot/blit/none/failure. `?` means missing history or no
runtime hook callback. The newest hand draws may not have reached Present yet
when V is sampled and can legitimately lack that callback.

## Validation completed

- `tools/desktop-eye-host.ps1`: **79,339 policy assertions passed**. Exhaustive
  6,561 length-eight sequences per lag (0 and 1), steady and repeated eyes,
  L/R/untagged bursts, startup, menu expiry, reset and failed copies.
- The legacy delayed-tag burst produced **9 right outputs and 7 eye switches**
  in the defined sequence. `--legacy-must-pin` deliberately exits 1; the harness
  verifies that rejection as the negative control. Its FAIL line is expected.
- Same harness: **72 assertions against the actual desktop_eye.cpp** compiled
  with a deterministic D3D9 device double passed. This independently supplies
  pixel identity, checks real copy direction and history, reproduces the legacy
  zero-tag leak, and refuses uninitialized surface reads across lifetime changes.
- `tools/build.ps1 -Release`: **passed**, 32-bit RelWithDebInfo candidate above.
  Visual Studio required SDK metadata access beyond the filesystem sandbox.
  Existing unity `DVR_CAT` macro-redefinition warning remains; final desktop
  module emitted no new warning.
- `frame_test.exe`: **passed** existing hand/weapon checks.
- `tools/exports-check.ps1`: **passed**, nine expected proxy exports.
- Entire generated ini literal compared with `tests/golden/dishonored_vr.ini`:
  **matched**, including the new key and comments.
- `tools/xrsim-selftest.ps1 -Release`: **passed** 60 empty frames on dvr-xrsim,
  reached FOCUSED, zero errors. This is the standalone smoke client, not the mod.
- Repository lint, new-file script/ASCII/line-ending checks and diff whitespace: **passed**.
- No successful in-game simulator validation, desktop perceptual verdict or
  headset verdict. Do not substitute the tests above for those missing results.

## Read-only evidence from the subsequent active run

After installation, an active game log was inspected without sending commands
or controlling the game. The source resolves to draw. At log timestamp
87284609, a 15,000 ms window reported:

```
draw L/R/0=900/900/15
tag same/opposite/zero=0/1785/30
singleTicks=15 snap/blit=900/915
oldShadowChanges=30 oldShadowRawLeaks=15
shownR=0 shownUnknown=0 warmupR=0 failures=0
```

This is a populated reproduction window: single-draw events occurred, the legacy
shadow would leak, and the new copy provenance stays left without failures. It
also directly supports the delayed/current identity mismatch. It remains a
counter-level result, not a visual or headset verdict. A partial log snapshot
is preserved locally at `build/vr76-validation/active-run-snapshot.log`.

## Next user-run tests

1. Launch the installed candidate normally. Confirm startup resolves
   `DesktopEyeSource=draw` and the expected build ID. Keep every prior setting.
2. Run A: watch the desktop mirror, press V immediately after any residual jump.
   A useful clean window has actual single ticks and oldShadowRawLeaks above 0,
   with no unexpected shownR, no copy failures and no visual mirror jump.
   No bursts means the reproduction question is inconclusive.
3. Run B: separate launch, judge the subtle headset symptom independently.
   If it persists, keep VR-76 open and examine the held-pair/burst behavior
   separately; this change does not alter headset image assembly or pacing.
4. Live A/B if needed: `tools/game-cmd.ps1 "desktopeye tag"` restores the
   legacy source; `"desktopeye draw"` re-enables the fix. Changing source drops
   the old snapshot deliberately. Do not score its first warmup presents as
   steady-state leaks. Without Save As Defaults, a relaunch still loads draw
   from the installed ini.

VR-77 single-draw generation and VR-75 cutscene behavior were not changed.
No game-derived captures or log artifacts are part of the source change.
