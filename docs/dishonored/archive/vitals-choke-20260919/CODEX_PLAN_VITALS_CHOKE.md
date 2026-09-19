# Codex implementation plan: vitals on the hand, calibrated choke, settings audit (2026-09-19)

Branch: `codex/hud-improvements` (draft PR #76, stacked on #75, which is NOT merged).
Tickets: VR-142 (vitals on the hands), VR-145 (physical choke), VR-144 (wheel
stutter, the ReduceDesktopPresent part), VR-141 (reticle, done). Read CLAUDE.md,
AGENTS.md and the top sections of `docs/dishonored/HUD_ANCHORS.md` first. Never
launch the game or the simulator: the tester runs every build. Build Release,
freeze each candidate with an `install.ps1` (copy
`build/playtest-candidates/hud-improvements/install-503/install.ps1`: it archives
both logs and the ini, checks the DLL hash, edits the ini byte-safely with CRLF,
and has a `-TestIni` dry run). Dry-run it on a COPY of the live ini before you
hand it over. One question per launch.

## 0. What the last run showed (run503, logs in build/playtest-candidates/hud-improvements/run503)

- The vitals are invisible everywhere. The attach step ran (`VitalsAttach.L/.R`
  end in `,palmF`), so `provide()` stopped submitting the XR quads for both parts
  (`vitals_scene_on()` plus a palm attach means skip). But `VitalsSceneDraw` never
  drew even once: the log has the two texture allocations
  (`hud/vitals-scene: part 0 texture 507x843`, `part 1 ... 558x843`) and NO
  `hud/vitals-scene: drawn` line. Its early returns (`g_vsPalmOk`, `ctx.ok`,
  `vitals_scene_cfg`, `vitals_scene_texture`, the once-per-eye gate) log nothing,
  so the log cannot say which one refused. Rule for the fix: never stop the
  fallback quad until the in-scene path has PROVEN it draws, and log every
  refusal with its reason.
- `hud/palm` printed only the LEFT hand in both logs, and 499's log once said
  "the right palm is not being drawn". Check whether the right hand's rotate
  branch reaches `MpPublishPalm`/`MpDrawnPalm` at all (the placed-range class
  test is `rng[r].cls == MS_CLS_HAND_B ? 1 : 0`, so check that mapping against
  `hIdx` in `MpWorldTarget`).
- Choke: `choke: START` fired twice and held RB for about 1.5 s each time, but
  the pawn never entered `StatePlayerMasterChoke`. `DishonoredInput.ini`:
  `m_PadBindingSet1 XboxTypeS_RightShoulder = GBA_Block` (block + choke), but
  `m_PadBindingSet2 XboxTypeS_RightShoulder = GBA_Primary` (attack). Find which
  pad set is active: log it at startup, it is a script property or a config
  value. Also confirm the right grip, which sends the same RB, actually chokes.
  If RB is not the choke on this machine, drive `m_bChokeButton` through
  whatever the grip really uses. Whether the choke is a HOLD or needs a fresh
  PRESS edge while "Choke" is prompted is unmeasured: the gesture sends one
  press edge at START and holds.
- Wheel stutter (VR-144): one-eye ticks while the wheel is open were 10% (470),
  25% (486), 47% (497), 28% (499). The tester cannot switch
  `ReduceDesktopPresent` off in F10 unless the desktop mirror is on.

## 1. Settings audit: one owner per thing (do this FIRST)

The tester sees many overlapping switches, all live at once. Live ini (run503):
- `Element.vitals=handR`, `Element.vitals.HandX/HandY/HandScale`, `Win*`
- `Element.vitalshealth.*`, `Element.vitalsmana.*` (HandX 0.084 / 0.078)
- `HandL.*` / `HandR.*` hand panels (billboard)
- `VitalsSplit=1`, `.Top/.Bottom`, `VitalsHealth/Mana.Crop*`
- `VitalsMirror=1`, `VitalsAutoCrop=1`
- `VitalsBack=1`, `VitalsBack.*` (the abandoned back-of-hand guess, still ON)
- `VitalsAttach=1`, `VitalsAttach.L/.R` (grip-framed or `,palm`/`,palmF`)
- `VitalsInScene=1`, `VitalsInScene.L/R.Trim*`
- `WheelSidePanels`, the wheel's own "Health and mana" part

Make ONE selector in the F10 HUD tab's "Health / mana" section:
`[Hud] VitalsMode = window | hand | attached | model` (default `hand`, which is
today's split-on-hand-panels behaviour):
- `window`: the vitals row on the window (no split).
- `hand`: split parts on the hand panels (the HandL/HandR panel + element offsets).
- `attached`: split parts as XR quads locked to the grip with the attach step.
- `model`: split parts drawn in the game frame on the drawn palm (section 2).
  Falls back to `attached` quads, visibly and logged, whenever the in-scene
  draw has not drawn for 250 ms.
Remove `VitalsBack` and its sliders, and the separate `VitalsInScene` checkbox
(migrate old keys once, as `configure()` does for the VR-117 keys, then delete
them on the next save). The selector shows only the controls of the chosen mode.
`hud/vitals: mode=<m> (owner <x>)` at load and on every change. Put a
`hud vitals status` seam word on the command seam listing the effective owner,
each part's anchor, and why a part is not drawn.

Also audit every F10 control that writes a key another control or mode reads.
In particular `Element.vitals` (the `hand` mode needs a hand anchor, but in
`model`/`attached` the vitals sink only needs to exist), the hand panel
orientations, and the wheel parts. Write the result as a table in HUD_ANCHORS.md.

## 2. Vitals on the hand model (`VitalsMode=model`)

Keep the design of candidate 503: a D3D9 copy of each part, drawn inside the hand
draw with that draw's own ViewProjection. It is the only approach that stays
locked through stance, animation and calibration. Fix and prove it:
1. Log every refusal in `VitalsSceneDraw` with its reason and counts (per 3 s):
   no palm for this hand, ctx not ok, no cfg, no texture (with the
   `vs_copy` reason), behind the eye, VB/state-block failure, draw hr.
2. A diagnostic that can fail: draw a solid magenta quad (no texture) at the
   palm with a fixed 5 cm offset, behind `hud vitals debug on`. If the magenta
   quad is not seen, the projection or the draw placement is wrong. If it is
   seen but the vitals are not, the texture copy is wrong (the sink RT may be
   empty at the copy point, or the copy lands after the clear).
3. Check that the copy happens while the vitals sink HOLDS this present's HUD.
   `vs_copy` runs in `end_frame` right after the StretchRect into the slot. If
   the sink was already cleared (`clearBeforeDraw`) or the redirect is not armed
   in that state, log it. Consider copying from the delivered D3D11 part texture
   instead (open a shared D3D9 texture) if the D3D9 RT is unreliable.
4. Placement without the XR round trip: capture the attach relative to the
   drawn palm in GAME space. At the end of the countdown, convert the frozen XR
   panel into camera-relative game space once (the inverse of the map in
   `MpPublishPalm`, at the rendered scale, for this present's eye), then store
   palm-local in game units. The `,palmF` flip handling then disappears. Keep
   the XR palm publish only as the countdown's visual and the log instrument.
5. The right hand: see section 0. Both hands must log a `hud/palm` line (make
   the 3 s log per hand).
6. Colours: the panel is drawn into the HDR scene before tone mapping. If it
   reads wrong, add a gain slider (`VitalsModelGain`) or draw after the
   post-process (later draw hook) as a follow-up.

## 3. Calibrated physical choke (VR-145)

The tester wants calibration like the vitals attach: do three chokes that feel
natural, and the mod sets the shoulder point and the speed.
- F10 Controls: "Calibrate choke (3 tries)". For each try: a countdown text in
  F10 plus a log line, then the tester makes the choke move and holds it for
  one second. Record the right hand's peak speed in the 600 ms before it
  settles, and the settled position in the head's YAW-ONLY frame (right, up,
  forward from the head; the same frame `shoulder_point` uses).
- Set: the shoulder point = the mean of the three settled positions, in the
  yaw frame (replaces ShoulderLeft/Down/Back). `EnterM` = max(0.08, the largest
  distance of a try from the mean + 0.05). `ExitM` = EnterM + 0.08.
  `MinSpeed` = 0.6 x the slowest of the three peaks, floored at 0.4 m/s.
- Save to `[Choke]` with `Calibrated=1`, and log every try and the result.
  Store the point as `ShoulderX/Y/Z` in the yaw frame (keep reading the old
  Left/Down/Back once for migration).
- F10 keeps the live readout (distance, speed, HELD) and the manual sliders
  under a collapsed "Advanced".
- Output: first establish what actually chokes (section 0). Candidates: RB as
  now; a fresh RB press edge; the pad set 2 layout, where the choke is on
  another button. Log the chosen output and the active pad binding set at load.
  Host-test the calibration math in `tools/choke-gesture-tests.cpp`.

## 4. ReduceDesktopPresent (VR-144)

The F10 control is greyed out while `[VR] DesktopMirrorOff=1`
(`src/core/ui/overlay.cpp` around line 834, `if (mirrorOff) ImGui::BeginDisabled()`).
But `desktop_eye.cpp` still reads `g_reduce` while the mirror is off (the skip
decision at its lines 215/229/253), and the tester's ini has both set to 1. So
the setting is live and cannot be turned off. Make the checkbox always
switchable, and say on it what it does while the mirror is off. For the
measurement, the installer can set `[VR] ReduceDesktopPresent=0` directly;
compare the wheel's one-eye ratio (`menu/head ... singles/writes` while the
wheel is open) with run499's 28%.

## 5. Order, candidates, questions

One candidate per question:
1. Audit + selector + the model-mode logging and magenta diagnostic, with
   `VitalsMode=model` and `hud vitals debug on` in the installed ini. Question:
   is a magenta square on each hand, and do the bars show?
2. The game-space capture, if the model mode draws.
3. Choke output research + calibration. Question: after calibrating, does the
   choke start on an unaware guard and hold until the hand leaves?
4. ReduceDesktopPresent=0 A/B. The question can ride with any launch whose log
   has the wheel open (read the ratio yourself).

Docs in the same commit as the work: HUD_ANCHORS.md (the settings table, the
model-mode record), CHOKE_GESTURE.md, PERFORMANCE.md (VR-144), STATUS.md.
Commits under the configured identity, no trailers, no names. Never merge.
