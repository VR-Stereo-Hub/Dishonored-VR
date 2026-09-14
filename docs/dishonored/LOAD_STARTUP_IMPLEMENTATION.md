# Load startup: implemented candidate, 2026-09-13

Status: implemented and host-tested; headset verdict pending. Branch
`codex/load-stereo-startup` starts at merged `20cc4a98`. PRs 50, 51 and 52
were independently verified merged. Their confirmed flicker and note fixes remain
in the baseline. Linear creation is pending because no connector was available
and the browser bridge failed to initialize twice; no ticket number is invented.

## Measured starting point

Saved run: `build/vr93-logs/vr98-run8-164007/dishonored_vr.log`, SHA256
`3eb6300b26f47e938cf393099bfb379cdc288a2d9cb9363aefe6a19d205c9a79`.
The observed symptom is a mono view after loading until jump/crouch/stairs, plus
hands tracking before weapons and a long freeze. This is separate from VR-80.

The log first latches the controller at 11114.421 s. The animation reader already
reads the possessed player at 11115.046 s, and equipment revision 1 arrives at
11115.609 s. The event pawn is first latched at 11124.375 s; crouch and GAMEPLAY
follow at 11124.390 s. The capsule getter used only that event pawn even though
other readers already obtained the controller's possessed pawn. This corrects the
handoff's tentative theory that all capsule readers were gameplay-gated: TrackHead
and the crouch readers can run earlier, but they were reading an unavailable pointer.

Large game-thread-wait gaps are 3339, 1794, 1281, 2106 and 2851 ms in the startup
window. Repeated reflected-name lookups and a 502 ms UI discovery occur within
those gaps. They do not account for every millisecond, and there is no measured
post-change claim about total load duration or instantaneous weapon tracking yet.

## Implementation

- `[Menu] PawnFromController=1` reads reflected `Controller.Pawn`, validates
  controller, player-pawn class and capsule against the live-object table, and
  measures capsule height without waiting for a pawn event. Null/unreadable or
  non-player possession cannot fall back to the old event pawn. A failed sample
  clears the old liveness lease. Main-menu, note, view-silence and cinematic gates
  remain in force; this does not force stereo while those gates refuse.
- A 50 ms script-lane sampler runs independently of head, hand and animation
  tracking. It refreshes a pre-load live-object table on failure, at most once
  per second, and retries unavailable reflection offsets. It does not synthesize
  input or write engine state to force a transition.
- The existing deep-crouch writer follows the same validated pawn. Camera clamping
  checks the identity of the pawn whose capsule was measured before combining its
  height with pawn Z. The confirmed camera offset/clamp reconciliation is retained.
- `[Menu] CacheNameLookups=1` is a separate timing experiment. FindNameIdx warms a
  bounded 512 KiB cache of positive hash/ID hints during its existing scans. Hits
  re-read the current engine name; no UObject/name pointers or negative answers
  are cached. Sixteen probes maximum; full/colliding buckets fall back to scanning.
  Cache access is serialized across lanes. This reduces repeated name scans, not
  all property/object discovery or engine asset loading.

Both settings ship OFF, load from the ini, have F10 Hands checkboxes, and persist
through Save As Defaults. Do not bump the config version or overwrite the tested
profile. Compare cold process launches for cache timing: toggling it after other
reflection caches are warm is not a cold-start control.

## Verification and remaining visual test

`tools/load-startup-host.ps1` compiles the production pawn reader, capsule writer,
script sampler and name lookup against synthetic engine data: 48 checks pass.
The old event source fails the no-input startup/ownership cases. With the actual
cache-off switch, the warm-scan performance assertion fails (4000 name reads);
with caching on, the same query takes one engine-name read. Cases include a
pre-load object table, unpossess, invalid class/component, NaN height, early
reflection failure, ID reuse, missing names later appended and cache saturation.

Other checks: 32-bit RelWithDebInfo build, 248 pairing checks, 43 menu-retention
checks, 19 camera-clamp checks, default-ini golden, nine DLL exports and lint pass.
No game launch or headset/simulator gameplay run was performed. There is an
existing DVR_CAT macro-redefinition compiler warning.

The first headset candidate enables PawnFromController only; CacheNameLookups
stays off so the stereo change is judged independently. Load while remaining
stationary: stereo should start without a jump/crouch once the existing view gates
allow it. Check a main menu, pause/note return, reload, crouch and descent. Then a
separate cold launch can enable CacheNameLookups to measure freeze duration and
first corrected weapon draw against the saved baseline. Neither improvement is
headset-confirmed until those runs are recorded. No new mainline merge is made.

## Installed candidate

Installed without launching the game. Active flags: PawnFromController=1,
CacheNameLookups=0; comparison of parsed settings found no other change.
Working DLL, ini and both logs backed up under
`build/load-startup/pre-install-20260913-171437/`.
Installed DLL SHA256:
`6b4bc561df98d3b668a05b958d6c97381c9cc59cebc03d1e41334da8c2e30651`.
Source is the uncommitted implementation on `20cc4a98`; backup/install manifest
and the source snapshot are under `build/load-startup/`. Restore the backed-up
DLL and ini together to recover the exact previous tested installation.


### Confirmed startup and note observer follow-up, 2026-09-13

1. **Symptom:** startup stereo no longer needs vertical movement, confirmed on the
   headset. Fast note opening/closing was reported delayed after a save load.
   This is the mono-transition surface, separate from eye-tag pairing.
2. **Reproduction:** build `vr33-hands-working-215-g20cc4a98-dirty`, banner verified
   against the install manifest before analysis. Archived in ignored
   `build/load-startup/playtest-20260913-173516/`; log SHA256
   `1d25a16094c878e93752e310b691ce26625fdb1bca126d617402c32142714172`.
   Installed ini SHA256 `5e1555d8e4e1b8bbe9e36e53715e5199ed64e4a8e192f7827c5f301e8349cdaf`.
   PawnFromController=1, CacheNameLookups=0, NoteFastMono=1; full ini diff confirms
   the prior note/pairing/menu options were preserved. Same 90 Hz profile.
3. **Hypothesis/counterprediction:** the append-only UI movie table exhausts across
   reloads, so new notes never reach the fast path. At 14397.578 s the old pNote
   is dropped; at 14400.265 s the table reports full at 48. Subsequent note close
   events occur without note flag transitions. If the replacement observer watches
   the new pNote yet the delays remain, table exhaustion is not the whole cause.
4. **Change:** the UI discovery scan replaces its population after refreshing the
   live-object table; new movies reclaim dead slots and validate class plus full
   FName against IsLiveObject. A table lock prevents partial replacement being read
   by Present, which skips a busy scan. Capacity is bounded at 128 for simultaneous
   movies; increasing capacity alone would not fix the lifetime leak. The existing
   NoteFastMono lever and loading timeout logic are unchanged. Logs identify watched
   notes and note-visible transitions. No new engine-memory writer or name-cache
   activation. Session instructions are persisted in AGENTS.md and private settings.
5. **Results:** 18 production-function checks pass, including 300 reloads, dead
   slots, address/FName reuse, full-live capacity, immediate note open/close and
   ordinary loading/observer-expiry safeguards. The old add-instance function fails
   four checks. This explains a repeatable missing-observer path; restored headset
   note timing still needs the next run. Startup is headset-confirmed; instant
   weapon tracking and total load-freeze removal are not established.
6. **Status:** startup confirmed; note reload repair awaiting headset verification.
   New Linear ticket/update is blocked in this task: no Linear connector is exposed,
   and browser startup fails with helper_sandbox_lock_failed. No invented ticket
   number, new commit, PR, or merge. The prior VR-98 issue remains the known context.


**Installed note-observer candidate:** `vr33-hands-working-215-g20cc4a98-dirty`,
compiled `Sep 13 2026 17:43:41`. The describe string is unchanged because this
working tree is uncommitted; verify the compilation timestamp as well as the tag
before interpreting the next run. DLL SHA256
`7a2dd73f97bb5fec655071198c335f70af2e2a8b74c8869f8e43ade7877c45ff`.
Backup and source snapshot: `build/note-observer/install-20260913-174444/` (ignored).
Release build and nine exports pass; pairing 248, menu 43, startup 48 and observer
18 checks pass. Ini golden and lint pass. The entire installed ini is byte-identical
to the preceding install, with CRLF verified; existing diagnostics remain armed.

**Next launch, one question:** after loading a save again through the pause menu,
do repeated note opens/closes still switch promptly between mono and stereo?
Load normally, reload the save, then open/close a nearby note several times.
Prompt transitions support restored note discovery across reloads. Delays with a
watched pNote and changing note-visible flag implicate a downstream gate; delays
without those markers implicate discovery. A flash or misplaced weapon indicates
a separate transition/retention regression. The agent reads the log; the tester
runs no commands. No game was launched during implementation.


### Reload right-eye displacement after note-observer repair, 2026-09-13

1. **Symptom:** fast notes and stereo without vertical movement are headset-confirmed,
   including notes after a save reload. A save reload caused continuous rightward
   flicker in the right eye until pause/resume. Route through section 1's one-eye
   pause/load/rearm row. This is not a repeat of the observer-capacity failure.
2. **Reproduction:** banner verified as build `215-g20cc4a98-dirty`, compiled
   `Sep 13 2026 17:43:41`, DLL `7a2dd73f97bb5fec655071198c335f70af2e2a8b74c8869f8e43ade7877c45ff`.
   Archive `build/note-observer/playtest-20260913-175403/` (ignored); log SHA256
   `fc78484e3310e9ca7eff4e5b9df60a32a118e3b3e901a6869a013b72399409ad`.
   Installed ini unchanged, SHA256 `5e1555d8e4e1b8bbe9e36e53715e5199ed64e4a8e192f7827c5f301e8349cdaf`:
   90 Hz, SharedWait=0, LateTagRepair=1, PawnFromController=1, NoteFastMono=1,
   CacheNameLookups=0. Reload returns GAMEPLAY at 15966.062 s; pause begins
   15989.109 s and GAMEPLAY resumes 15990.703 s.
3. **Hypothesis/counterprediction:** repeated SINGLE ticks expose adjacent R/0 labels
   on L/R images. Of 288 unique ledger rows sampled in the reload episode, 23
   popped explicit zero tags, all promoted to right by the within-tick invariant;
   22 had a preceding R-tagged unknown step with a 6-7 uu camera residual. Example
   P4588-4591: L matches its camera, R repeats the left camera (6.79 uu from its
   record), zero has the -6.82 uu right step, then L matches again. No ring rejects,
   clears, late owes, or repairs occur in the fully covered 15972.953-15982.968 s
   window; every mutation reconciles. Queue depth 4-6 is measured, not a cause.
   Earlier suspicion of depth-clearing is excluded. The submitted +1 doublets
   raise the runtime's LEFT-stale count even though the reported moving image is
   RIGHT: a mislabeled left view enters R before the real R, so that counter's
   eye is not the symptom's eye. The engine-side owner of the displacement is open.
4. **Change:** `[Stereo] SingleTagRepair` defaults off with a persisted F10 Display
   toggle. A one-present candidate requires a preceding measured L, an R-tagged
   camera repeat within 0.1 IPD, and a stored R position one IPD to its right. Only
   an adjacent zero tag with the robust right step and camera matching that stored
   R position confirms it. The strict tolerance is a conservative candidate guard,
   not a calibrated engine constant. A serial/record-checked buffered R capture is
   retired to untagged HOLD, and the current image gets that complete R record.
   No tag search/drain, camera write, or forced alternation. Sync/SharedWait=1 and
   identity mismatches decline; failed capture retirement leaves labels unchanged.
   Lifecycle reset clears the candidate. The log counts observed/fixed/refused.
5. **Results:** host replay of the observed geometry passes 23 checks, including
   wrong-eye suppression, full pose-record recovery, nonadjacent/moving/missing
   camera negatives, and capture serial/record/mode guards. Disabling the candidate
   reproduces wrong-eye delivery and wrong record. Pairing 248, note observer 18,
   menu retention 43, startup 48 checks pass. No simulator game launch (tester owns
   launches); headset validation of this candidate is pending. Note transitions in
   the recorded run reached the state machine in 0-16 ms, including the post-load
   note. No claim that every render surface is correct from the camera trace alone.
6. **Status/remaining:** note observer fixed and confirmed; reload right-eye repair
   is a candidate. Next launch asks whether a save reload stays stable without a
   recovery pause. If it persists with zero candidate observations, the guard/path
   missed it; fixed counts with residual flicker point beyond this sequence.
   Weapon tracking freeze remains separate: startup gaps 3341, 1811, 1266, 2141,
   2852 ms, mostly game-thread waits, with one render execution gap; discovery scan
   498 ms. Name-cache optimization remains OFF for this isolated flicker test.
   Linear remains unavailable in this task; no new number invented, commit, PR,
   or merge. Existing VR-77 SINGLE scheduling and VR-99 residual timing are related
   context, not a claim that either ticket already contains this finding.


**Reload candidate installed:** same describe tag `215-g20cc4a98-dirty`, banner
compile timestamp `Sep 13 2026 18:01:15`. DLL SHA256
`27e691b26829f852da59c998187d3139a653d7317ff7dcc28067398617d70162`;
ini SHA256 `2e14a8f0e42145e7cae96f8bd10cdd4ccd508cc4ac78b0255e4198ba9a8cd365`.
Backup, full ini diff and source snapshots are under ignored
`build/single-tag/install-20260913-180438/`. Only installed ini addition:
`[Stereo] SingleTagRepair=1`. Full section/key diff, byte verification and CRLF
checks passed. Startup cache stays off; note/pawn/late-tag settings stay enabled.
Release build, nine exports, lint and ini golden pass. Candidate headset result
pending; the agent did not launch the game.

**One launch question:** after loading normally and reloading the save once,
does the right eye remain stable for about 20 seconds without a recovery pause?
If stable, correlate with single-tag observed/fixed/refused counts. If it flickers,
let the episode run briefly, then pause/resume as before; counts distinguish a
missed/refused candidate from flicker that persists despite confirmed repairs.
Do not combine this launch with the startup-cache timing A/B.


### Reload confirmed; pause crash recurred, 2026-09-13

1. **Symptom:** the tester reports correct startup stereo, fast notes and stable
   stereo after a save reload. Opening pause at the end crashed before the menu
   appeared. The successful eye result and the GC crash are distinct observations.
2. **Reproduction:** banner verified against the install manifest before reading
   the run: `vr33-hands-working-215-g20cc4a98-dirty`, compiled
   `Sep 13 2026 18:01:15`. DLL SHA256
   `27e691b26829f852da59c998187d3139a653d7317ff7dcc28067398617d70162`;
   ini SHA256 `2e14a8f0e42145e7cae96f8bd10cdd4ccd508cc4ac78b0255e4198ba9a8cd365`.
   Logs, screenshot and dump preserved under ignored
   `build/single-tag/playtest-crash-20260913-181413/`. Current log SHA256
   `9501db99d75a730c6533a1fe8608f34069ef95556534a5ad4b54ee587482770e`;
   dump SHA256 `e78c3c3887f6ed01a6d7cd7296e628f36553ff9f2273aa6f41b8eba5db08fe4e`.
3. **Hypothesis/counterprediction:** the adjacent R/0 repair should suppress the
   previously measured wrong-R capture. A visible failure with confirmed repairs
   would put the remaining fault beyond that sequence. The pause crash matches
   historical VR-96; its timing alone cannot identify a writer or establish that
   the new stereo repair caused it. The 1.0 bit pattern alone is not attribution.
4. **Change:** no code, DLL or installed ini change during crash triage. Existing
   SingleTagRepair=1, LateTagRepair=1, NoteFastMono=1 and PawnFromController=1
   remain active; CacheNameLookups=0. Updated the evidence and priority handoff.
5. **Results:** ten note cycles, five before reload and five after, reach the
   corresponding LOADING/GAMEPLAY state in 0-16 ms. The only printed single-tag
   result is observed 1 / fixed 1 / refused 0 at 17147.625 s, for R draw/record
   2113, before reload. Reload returns GAMEPLAY at 17171.250 s and is reported
   stable through the final pause at 17246.765 s (including intervening notes).
   Do not infer that the original fault recurred after reload from this visual
   pass. At 17246.828 s the first AV reads 0x3F800008 at engine RVA 0x65894;
   its EAX is float 1.0's bits. The dump is written 125.672 s later and its
   exception is engine RVA 0xAF6A73 reading 1. The initial reference slot and
   token storage are absent, so the dump cannot identify the corrupting object.
6. **Status:** tested reload stability and fast notes confirmed for this build;
   VR-96 is the first-priority unresolved crash. Startup weapon freezing remains
   open. No crash fix or additional test is claimed. Linear updates are prepared
   but not sent: no callable Linear tools and UI runtime startup failure. No
   new ticket number invented, commit, PR or merge. See ENGINE_NOTES for the
   initial-fault evidence and the limit of the delayed dump.


## First-fault GC diagnostic installed, 2026-09-13

The current delayed minidump cannot identify the GC reference owner. Added an
opt-in `[Diagnostics] GcFaultDump=1` path: config verifies the six instruction
bytes in patterns.h at the known fault site and arms the core crash handler.
Only an AV/read with both exception address and context EIP matching that site
captures full process memory and the original exception context, once per run.
Capture runs before stack fingerprinting and its three-message budget. It then
continues normal exception handling; no engine-memory write, GC skip, exception
recovery or change to stereo/note behavior. Unrelated faults retain old behavior.
Byte mismatch refuses and disarms the diagnostic. The option defaults off and
is not inserted into generated/saved defaults. A full dump may be large and adds
writing time only when the targeted fault occurs; it stays local under the data
folder and must not be committed.

`tools/crash-capture-host.ps1` compiles the production handler in a standalone
32-bit process: 18 checks pass, including off, byte mismatch, unrelated exception,
write/execute, missing context, wrong site, teardown, exhausted logging budget,
once-only capture and original context/flags. Its actual Windows dump was read
back: original EAX/EIP and a reference-address heap sentinel are present, with
full-memory output 23,262,847 bytes. This tests evidence capture, not crash removal.
Release build, nine exports, ini golden generation and lint pass. No game launch.

Installed build `215-g20cc4a98-dirty`, compiled `Sep 13 2026 18:27:13`.
DLL SHA256 `1a6e3156c6c994f3dd57a401e44ffe7d1f7b59f12295cbb51c877073f7250725`;
ini SHA256 `d47a790a97862653d740436d939c155d2299d9931a4a445ca4af557b99a97782`.
Backup and complete ini diff: `build/crash-triage/install-20260913-182848`.
The only installed ini change is `[Diagnostics] GcFaultDump=1`; full section/key
comparison and byte checks confirm CRLF. Confirmed note, startup and reload-eye
fixes stay enabled; startup name cache stays off. Linear remains inaccessible in
this task; no new ticket number, commit, PR or merge.

One launch question: does opening pause after a save reload still crash? Load
normally, reload the save once, play for about a minute, then open pause. If it
crashes, the initial-fault dump should preserve the bad reference and surrounding
object memory, allowing owner/property identification and a targeted writer fix.
If pause opens, this intermittent fault did not reproduce; it does not establish
that the crash is fixed. If a different fault occurs or capture fails, use its
first log fingerprint to choose the next step. The tester only launches/reports;
the agent reads the build banner, archives evidence, and analyzes it before any
relaunch. No additional note/performance question is combined with this test.


### Final headset verification and default promotion, 2026-09-13

The verified 18:43:51 build completed a clean run with 11 pause openings and no
exception. The crawl writer refused nine stale control updates over three edges
and performed 57 updates to validated controls over the remaining edges. Thus
the stale-pointer guard was exercised, rather than merely failing to reproduce
the trigger. The tester confirmed normal behavior. VR-96 is fixed for this
observed cause; later distinct crash signatures must be investigated separately.
Archive `build/crash-triage/playtest-pass-20260913/`; log SHA256
`a082eae06a07fb3fcbba4656f8933d82232e9b55f06d656469b6338a249bd6be`.

The maintainer explicitly approved commit, PR and merge of the current branch,
and promotion of the complete installed settings/F10 profile to repo defaults.
This supersedes the earlier default-off disposition for these tested levers.
`release/dishonored_vr.ini` is the byte copy of that installed CRLF profile;
WriteDefaultIni and the golden match, including diagnostics, calibration values,
D:\dvr-data, fast notes, late/single-tag repair and retained menu identities.
The name cache stays off. No config version bump rewrites existing settings.
The real default writer runs in a standalone x86 host and its output is compared
byte-for-byte to the installed and packaged files. Package generation now includes
that exact ini. Presence/migration sentinels remain distinct from value defaults.
No release or milestone is declared. Linear API synchronization remains pending;
the PR's Fixes link may update VR-96 through the integration, to be verified next
session. The next feature is physical head movement during cinematics.
