# Roadmap

The mod starts SHIPPED (alpha 38.92 works on SteamVR headsets). Every milestone below is
"done when" a measured effect holds, not when code lands. Milestones after D1 run against the
simulator first (`docs/VERIFICATION.md`) and the headset last.

## D0 - Refactor and harness (session 1, 2026-09-02)

- [x] CMake + MSVC build, win32 preset, static CRT; `d3d9.dll` exports == the nine golden names
- [x] DXVK fork restored under `dxvk/` as commits; `dxvk-shipped` tag == the M8.2 the proxy targets
- [x] Module tree with verbatim bodies (`tools/split-source.py --check`)
- [x] Core utilities as real modules: log (levels, tags, ring, rotation), crash (fingerprint,
      minidump), clock, mem, ini, paths, vtable/IAT/detour hooks
- [x] Debug surface: command seam + ack, status.json, frame dumps, overlay Log tab,
      `[game] state:` line
- [x] `patterns.h` holds every address; `src/legacy` gated by `DVR_WITH_LEGACY`
- [x] Backend probe (runtime capability) replaces the process snapshot
- [x] Simulator + smoke client build; `xrsim-selftest.ps1` PASS
- [x] Harness scripts adapted (no `-Game`, Steam path resolution, x86 guards)
- [x] Docs set + CLAUDE.md
- [ ] Table-driven config (`core/config`): every key in one table, `write_missing` adds new
      keys to users' inis without touching values; golden ini diff = old + new keys only
- [ ] `IVrBackend`: OpenVR and OpenXR behind one interface; no `g_xrOn`/`g_sys` switches
      outside `core/vr`
- [ ] Dissolve `src/mod/state` into module-owned state; cross-thread members atomic
- [ ] DXVK fork built on the dev PC (`tools\build-dxvk.ps1`), export check 15/15

Done when: both configurations build, exports 9/9, fork exports 15/15 (+`dxvk_vr_view`
optional), ini golden diff empty modulo the em-dash fix, selftest PASS, lint clean.

## D1a - Port the author's 39.x fixes (docs/dishonored/HANDOFF-GINGASVR.md)

One behavioral change per commit; each verified against the handoff's log signatures.

- [ ] Obtain `dllmain_38.72.cpp`, `dllmain_39.4.cpp` and the fork's p53 commit from the author's
      archive; diff 38.72 vs 38.92 and 38.92 vs 39.4
- [ ] 39.3: create the D3D11 device on the adapter LUID the OpenXR runtime returns
      (`D3D_DRIVER_TYPE_UNKNOWN` with an explicit adapter; `dxgi.dll` via LoadLibrary); log
      `xr: the runtime requires D3D11 on adapter luid ...` and `*** WRONG GPU ***`
- [ ] 39.4: the menu-ghost quadrant (`menuOpen && cursorVis` covered by neither rescue)
- [ ] 39.2: the pitch kept/discarded closed loop (120-frame windows; stop claiming the camera
      when the engine discards, so the fallback engages)
- [ ] 39.0: calibration records banked by asset name (`FpBankFind/FpBankStore`), not by
      component pointer
- [ ] Keep the 38.78 focus keep-alive (present in our base, missing in the author's 39.x)

## D1 - Baseline parity in the headset (needs the game; user, SteamVR lane)

- [ ] `tools\install.ps1` with the shipped fork; boot to gameplay; `dishonored_vr.log` shows the
      same hook installs (ProcessEvent, Blink x3, pad, res spoof) and splice counts as a
      38.72/39.4 log from the author's rig (38.73-38.92 stacked unverified changes)
- [ ] Hands calibrate; Blink hand-aims; wrist HUD shows; F5/F10 work; no new WARN/ERROR lines
- [ ] `tools\boot.ps1` reaches `[game] state: GAMEPLAY` unattended (fix the key sequence on the
      first attended run)

Done when the user signs off in the headset and `tools\log-parse.ps1` shows no regressions.

## D2 - Diagnostics proven live

- [ ] `game-cmd.ps1 "status"` -> `status.json` with a live pawn; `dump eyes` writes two PNGs
- [ ] `xrsim-launch.ps1` -> `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`; `smoke.xrs`,
      `stereo.xrs`, `headlook.xrs` pass
- [ ] `soak.ps1 -Minutes 10` exit 0 on the simulator

## D3 - OpenXR/Quest convergence (`docs/dishonored/XR_HANDOFF.md`)

- [ ] An affected Quest user runs a build with the 39.3 adapter fix; read the adapter lines in
      their log; collect GPU vendor / model / driver (never collected once)
- [ ] Verify 39.4 in a real pause menu (head-look must stay parked) and 39.2 on an affected
      machine (does the fallback drive correctly once it engages?)
- [ ] One layer mode and one stamping policy chosen by measurement in the simulator
      (`world-6dof.xrs`: ClaimRatioH ~1.0 at every yaw/pitch); the other 5 switches retired
- [ ] Hands + wrist HUD + overlay drawn in every XR presentation mode
- [ ] `dxvk_vr_view` re-derived in the fork or StampFix removed
- [ ] A Quest/VD user confirms no warp and no head-lock after load

## D4 - GPU frame sharing

- [ ] The fork exports a shared-handle surface; the proxy opens it in D3D11
      (`OpenSharedResource`) and the 36 MB/frame CPU readback goes away
- [ ] `@fps` in the simulator shows the present-thread cost drop; `img-diff` of the eye
      images vs the readback path at the noise floor

## D5 - Hands decoupled from the head

- [ ] The `LookAtControl_Camera` / donor-graft lane (ENGINE_NOTES "head coupling") measured:
      head orbit with hands static -> hand screen bbox stable (`coupling-hand.xrs` style)
- [ ] `GraftHeadFollowYaw/Pitch` converged by measurement, not the 1.5 guess

## D6 - Prologue and cinematics

- [ ] The prologue block fixed at the source; `IntroSkipApply` retired
- [ ] Head-look in cutscene cameras

## D7 - Hand-aimed powers

- [ ] Possession, Devouring Swarm, Windblast on the Blink aim lane

## D8 - Presentation polish

- [ ] Per-eye light consistency, thin-object shimmer, menu-on-wrist sizing, vent/crouch glitches

## D9 - Release

- [ ] `tools\package.ps1` zip; RELEASE_NOTES; GitHub release; `Version=9` inis still load

## Post-release backlog

- Proxy-level stereo (Mirror's Edge VR style draw duplication) evaluated against the fork
- OpenXR-only architecture with the SteamVR shim (`bvr_steamvr32`) if the native OpenVR path
  becomes a maintenance burden
