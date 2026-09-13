# Investigation plan: frame-drop world ghosting and mono-resume weapon settling

Created 2026-09-13. Starting source: `1d5deb9b`. Status: source/log investigation saved;
no renderer change, install, or game launch authorized by this plan itself.
The user requested investigation and a recoverable plan first. Keep results here
and in [FLICKER_REFERENCE.md](FLICKER_REFERENCE.md) as work proceeds.

## Reported observations (not yet log-correlated)

1. Fast head turns sometimes produce a one-frame ghost/doubled edge on world
   geometry. The suspected correlation is an actual frame drop or variable frame
   rate; it has not yet been measured for this report.
2. Entering gameplay produces roughly one second of weapon flicker before stable
   lock-on. Equipment swaps retain lock. Menus that switch rendering to mono
   reproduce the settling interval when stereo gameplay resumes.

Question: can persistent identification/calibration survive a same-world menu so
only initial startup needs learning? Distinguish relearning from stale-eye output,
missing fresh pose/component samples, and stereo re-arm before deciding to cache.

## Work order and checkpoints

- [x] Save this plan before extended investigation.
- [x] Add mandatory first-read/update rules to CLAUDE.md.
- [x] Map world-image delivery, pose generation, pair/hold behavior, and pacing
  around frame gaps. Separate normal repeated-image temporal judder from a wrong
  eye, stale pose, missing layer, or desktop-only leak.
- [x] Map every weapon/candidate/calibration reset on gameplay -> mono -> gameplay;
  compare pause/note transitions with real load, pawn replacement, device reset,
  equipment swap, and tracking loss. Name each cache's owner and safe lifetime.
- [x] Inspect existing local run artifacts read-only for matching build/settings
  and transition evidence. Do not treat an unrelated log as this reproduction.
- [x] Produce ranked, falsifiable hypotheses and a minimal implementation plan for
  each issue. Document what data is missing before proposing a headset run.
- [ ] Update the reference with source-backed findings, current status, and an
  exact continuation checkpoint. Validate documentation links/formatting.

## Issue A: world ghosting on a dropped frame

Check capture delivery serial/tag/pose transport, xrWaitFrame pacing, first-eye
pairHold returns, held-layer fallback, swapchain release age, and the image pose
used for reprojection. Read both eyes' populations. A repeated old frame with its
correct old pose is different from old pixels relabeled with a new pose.

Evidence needed: a marker immediately after a visible event; per-present timing,
actual output and held image identity, eye/pair generation and submitted pose,
resolved capture mode/SharedWait, refresh, lag settings, and runtime. Preserve
90 Hz/lag-2 tested controls while attributing; do not silently switch settings.

Counterpredictions: if complete pairs and image/pose joins stay correct while
frame time crosses a display slot, the first candidate is scheduling/repeated
imagery, not c5 or weapon placement. If one eye/release/pose diverges at the event,
repair that specific ownership boundary. Do not claim fully smooth variable-rate
rendering can be achieved merely by retaining an older frame.

## Issue B: weapon reacquisition after a mono menu

Classify retained state into persistent identity/calibration and transient output.
Potentially retain same-world, live component/geometry contracts and calibration;
never replay old per-eye correction matrices, bone/device constants, stale head or
controller samples, or raw pointer identities across a destroyed owner/resource.

A safe proposal must distinguish temporary menu suspension from world/owner/device
replacement, then validate retained records before consuming them on resume.
If fresh samples or stereo pairing, rather than contract learning, explain the
one-second interval, caching contracts alone will not cure it.

Counterpredictions: contracts invalidated/re-adopted around menu close support
reacquisition. Contracts/calibration unchanged but eye ages/unknowns/refusals spike
support a rendering or freshness gate. Stable lock through swaps is useful
negative evidence against an ordinary per-weapon ownership bug, not proof.

## Verification plan (after a concrete change is selected)

Use the existing production host harness for a deterministic defect and make the
old behavior fail. New lifetime tests should cover same-world pause retention,
real load/pawn/device invalidation, equipment changes during suspension, partial
snapshots, recycled addresses, and resume before fresh samples exist. Image/pose
changes require per-eye identity and release checks, including a missed deadline.

The visual acceptance matrix separates headset left/right from desktop, fast and
slow head turns, steady and dropped frames, cold startup, pause/note return,
weapon swap, reload, and actual level transition. Preserve old logs/configuration.
No live launch or installation is needed to finish the present investigation.

## Continuation checkpoint

Initial checkpoint (superseded by findings and exact restart checklist below):
Plan saved. Next: read `WaInvalidateContracts` and all callers, candidate resets,
`MpPaletteOnReset`, mono method transitions, and runtime hold/image-pose paths.
The initial source search already found a comment saying contracts are dropped
when the game leaves gameplay because a load destroys components; determine
whether that broad trigger also catches a temporary mono menu.

## Investigation findings saved 2026-09-13

Status: the read-only source/log investigation is complete for this pass. No
renderer fix is implemented. The next steps below are implementation proposals,
not claims of a successful visual fix. Start here when resuming this task.

### Preserved evidence and scope

A snapshot of the latest available installed log is saved locally at
`build/flicker-resume-investigation-2026-09-13/observed-run.log` (ignored artifact).
SHA-256: `BB414A99CA4BF60A9660F093241BD527F7AB177523CF3B487D62401E565EFA74`.
It identifies `vr33-hands-working-182-gfa2ea4e3-dirty`, built September 13 at
10:17:14, not the plan's clean source baseline. It is supporting evidence from an
existing run, not a reproduction arranged for these two reports. No V markers
exist in this snapshot. Its exact dirty patch and perceived event times are not
known; do not attach a visual verdict to a frame solely because it appears here.

Resolved settings include world Lag=2, Hands PoseLag=2/PoseLagAb=0, Ahead=0,
Strict=0, SyncHz=0, 90 Hz, and shared capture delivering the previous slot
(SharedWait=0). Current desktop source is draw. No live commands were sent.

### B1. Ordinary menus definitely destroy weapon identification

Source: [commands.cpp](../../src/game/dishonored/commands.cpp), `GameStateTick`.
The transition condition is `wasGameplay && !nowGameplay`. It unconditionally
calls `SuBeginLoad`, `UiNoteLoad`, `WaInvalidateContracts`, and
`FpInvalidateCandidates`. Destination MENU, CINEMATIC, LOADING, and NO_PAWN all
share the same branch. A temporary pause is therefore processed as a destructive
load for these caches even when its pawn is still live.

Source: [weapon_attach.cpp](../../src/game/dishonored/hands/weapon_attach.cpp),
`WaInvalidateContracts`: zeroes the contract array/count and held-item objects.
[fp_mesh.cpp](../../src/game/dishonored/hands/fp_mesh.cpp),
`FpInvalidateCandidates`: clears candidates/selection and write pointers, resets
retry/settle counters and equipment revision, and marks the list dirty.
`FpEnsureCandidates` rebuilds on the script lane, with a 500 ms retry gate;
child discovery has separate equipment-settle passes. These are real resets,
not a hypothesis inferred from the user's word lock-on.

The preserved log contains an ordinary pause and resume:

| Log timestamp (ms) | Event | Implication |
|---|---|---|
| 10167640 | `Dis_OpenPauseMenu`; one contract and three candidates dropped; MENU with pawn=1 | The broad reset executes on a live-pawn menu |
| 10168843 | `OnResumeGameClicked`; state becomes LOADING | LOADING here is a heuristic intermediate state, not proof of level replacement |
| 10169390 | First live dispatch; GAMEPLAY | The existing menu-silence bypass takes effect |
| 10169906 | Candidate list rebuilt, three components, one attempt | Identification inputs were discarded and rebuilt |
| 10169921 | DOUBLE after 158 single ticks; a 538 ms frame gap reported | Resumption includes a large script-side stall |

Important limit: the final two weapon summaries show 0/64 contracts, with no new
adoption recorded before the snapshot ends. Equipment visibility/handback also
varied earlier in this run. Thus the log confirms invalidation/recollection,
not the duration of visual weapon relock and not a measured one-second adoption.
The user's repeatable startup/menu observation supplies the symptom; a dedicated
resume trace must still time first valid weapon correction with a drawn weapon.

Stable weapon switching already has the analogous partial solution:
`WaRetireContractsNotIn` retains merely stowed contracts. Its comment explains
that destroying those identities previously caused reacquisition on every swap.
The menu path bypasses that retention and deletes the entire table.

### B2. The same broad transition schedules an expensive diagnostic rescan

`UiNoteLoad` sets `g_uiRescan`; `UiTick` calls `UiDiscover` on the script lane.
In [arms_hide.cpp](../../src/game/dishonored/hands/arms_hide.cpp), the order is
`RflTick -> UiTick -> FpEnsureCandidates -> WaCompTick`. Consequently, a slow UI
scan delays the fresh component publication needed by weapon placement.

The ordinary-pause example above logs `UiDiscover` scan #2 taking **516 ms**,
ending at 10169906, immediately before candidate rebuilding. The frame gap is
**538 ms**, with **525.8 ms** attributed to waiting for the game thread. This is
strong source-and-timing evidence of avoidable scan work in the resume interval;
it does not attribute every millisecond to that scan or prove it causes every
visual flicker. The startup scoreboard reports a 2.28 s interval from menu open
to stereo return, including 1.21 s spent with a menu up. Its label of all 1.74 s
before the view verdict as mod-owned delay is overbroad for this pause population.

[ui_state.cpp](../../src/game/dishonored/ue3/ui_state.cpp) describes the movie
verdict as a proposal/observer. `[Menu] UiProbe` currently defaults to 1 and
`uistate on|off` exists. Turning it off could isolate scan cost in a controlled
comparison, but is not a fix for contract destruction and was not done here.
Do not alter the production gameplay predicate to use the experimental movie
count; the reference documents why that does not reliably identify menus.

### B3. What can be cached, and what cannot

The answer is conditionally yes: preserve already-learned weapon/pass identity
across a verified same-world menu, then refresh its live inputs before reuse.
The cache already exists as `WaMesh`; the current transition throws it away.
There is no evidence that a new disk cache is needed.

| Retain only with validated lifetime | Refresh or invalidate before resumed consumption |
|---|---|
| Geometry/pass contract key (buffers, ranges, shader/declaration) | All device-dependent identity after Reset or device replacement |
| Component association, hand/asset assignment, native-vs-rebased match mode | Membership and component-to-snapshot index against a fresh snapshot |
| Still-valid source calibration under the same source generation | Calibration after palette/source/layout generation changes |
| Same-owner candidate identities or a validated retained registry | Component transforms, controller/head poses, and snapshot timestamps |
| Live stowed contracts | `dm/dmPresent/dmOk`, common hand correction, held-reference positions/ages, old per-view comparisons |

`MpOnReset` deliberately clears contracts, layout/palette caches, common
corrections, calibrated origins, and source validity while incrementing
`g_mpSrcGen`. Preserve those hard boundaries. Do not carry raw buffer addresses
or camera-relative transforms across a reset, load, or process restart.

`WaRetireContractsNotIn` currently uses `LooksLikeObj` as its cheap check. That
is not a sufficient new cache lifetime guarantee. The repo's `IsLiveObject`
reads a locked GObjects-derived live set, but membership in a cached set or an
unchanged pointer also does not independently prove that an address has not been
reused. Revalidation needs a current live set plus controller/pawn/component and
owner/asset identity checks, equipment revision, and resource/source epochs.
If no trustworthy world/owner lifetime discriminator is available, retain records
only as suspended candidates and refuse them until fresh engine evidence agrees.

Do not implement this as simply deleting the two invalidation calls or exempting
all MENU states. Loading a save can start from a menu, a load can preserve/reuse
addresses, and a resume can briefly read LOADING without a real load. The
suspension must survive that benign intermediate state while hard invalidation
still detects real destruction. Freshness/ownership gates remain mandatory.

### A1. Actual frame gaps exist, but the visible ghost has no event join yet

The snapshot contains 46 `perf: frame gap` records. Classified by the latest
logged state at the time of the report, 29 are GAMEPLAY and range from 48 to
538 ms: 23 in present-tail/xrEndFrame, three out/idle waiting for the game thread,
one pre_tick, and two in the game's Present. This classification includes the
large just-resumed menu gap; it is not 29 proven steady-play visual glitches.
The remaining records occur under NO_PAWN, MENU, or LOADING.

Rate windows include uneven intervals even at the already-configured 90 Hz,
with examples around 1.06-1.11 display slots per pair and several milliseconds
of interval variance. Selecting 90 Hz again is not an investigation of this
remaining issue. Likewise, the phase line sometimes closes well before predicted
display time while intervals are uneven: meeting a prediction horizon and filling
every display interval are different measurements.

The 23 xrEndFrame gaps locate a cost at the runtime call. They do not prove
Wi-Fi, encoder, driver, runtime pacing, or any particular downstream owner.
Long gaps elsewhere also exist, so a blanket streaming diagnosis is unwarranted.

Available `stereo: eyes` summaries contain no nonzero abort or stale-eye fields;
late windows report healthy ages 1/0. That makes a sustained one-sided stream a
weaker explanation for this snapshot, but is not pixel/event proof. The printed
`posesub` audit is **98 right-eye lines and zero left-eye lines**. It cannot clear
both eyes on the brief event. There are no V markers to correlate with these gaps.

### A2. Source-level candidates, ranked by next useful test

1. **Irregular fresh-frame delivery / missed display opportunities.** Directly
   supported by the gap and cadence records. First join a visible event to a
   bounded per-present history, rather than changing c5 or weapon placement.
   If image/pose identity is sound through the event, work on the measured stall
   owner or delivery cadence. Retaining an old picture avoids some discontinuities
   but does not create new world/animation frames; perfect variable-rate smoothness
   is not a result this plan can promise.
2. **Fixed lag ceases to match an image during a timing disturbance.** In
   [openxr_runtime.cpp](../../src/core/vr/openxr_runtime.cpp), the SR capture path
   chooses current/content/previous-two view arrays by numeric lag, then stores
   the selected pose in `g_eyePose`. Capture already transports `delivered_rec`,
   whose record holds the camera-consumed sample/pair/eye, but that record is
   used in a later diagnostic, not to select the submitted pose. This is a real
   identity boundary to test under a stall; it is not proof lag 2 is wrong during
   the reported event. Generation labels in the present audit can differ even
   when orientation is numerically equal, so do not fix a label discrepancy alone.
3. **Hold/release image versus pose mismatch.** The saved-layer structure names
   the latest released swapchain images, not frozen pixels. Retest only if the
   event history demonstrates an actual partial/new release followed by a hold.
   The old parked held-pair hypotheses failed their counterpredictions; do not
   revive them merely because the current report mentions a frame drop.
4. **Desktop-only or transition-only artifact.** Confirm the affected surface.
   Current draw-source mirror counters show successful copying during populated
   burst windows. Menu unknown output is expected after the bounded hold expires.
   If the event is truly headset world geometry, a mirror change is not the fix.

Also inspect acquire/wait/copy failures if the event records any. The runtime's
copy-ready branch and pose/validity bookkeeping do not have identical guards;
that is a conditional review target, not an observed cause in this snapshot.
Do not expand this task into a speculative runtime rewrite without a populated
failure case.

## Concrete implementation sequence proposed

### Patch B1: suspend and revalidate weapon identification across menus

Priority: first behavioral candidate, because both the code and a real pause
log demonstrate needless destruction of learned identity.

- Introduce a small explicit lifecycle distinction: active, suspended menu,
  resume validation, and hard invalidation. Have the present lane publish a
  transition/reason/epoch request and the owning script/render lanes consume it
  at their safe boundaries. Do not add another latest-global-per-draw shortcut.
- On a recognized temporary menu with a validated existing owner, suspend
  correction use and retain eligible contract identities. Do not keep old
  per-view output valid or refresh its age artificially.
- On resume, obtain fresh equipment/component state, validate owners/resource
  epochs, remap snapshot indices, clear transient references, and resume normal
  matching/verification. New/changed components still use existing discovery.
- Hard invalidate on verified owner/world loss, invalid component membership,
  relevant resource reset, or incompatible source generation. Preserve load
  recovery, bounded child-settle retries, stowed-contract retention, and all
  world-instance vetoes.
- Use a default-off live A/B lever for the new retention behavior and bounded
  transition logging: reason, owner/epoch, retained/invalidated count, first
  fresh snapshot, first corrected draw, and refusals. Runtime defaults change
  only after the normal measured/visual validation process.

Acceptance: ordinary same-world pause/note resumes avoid contract re-adoption and
correct the first eligible fresh stereo weapon draw without native-position
copies or missing lighting. Cold startup can still learn. Real load and device
reset must discard stale identity and recover normally. A closed menu cannot
promise literally zero warmup frames when tracking or fresh draws are unavailable.

### Patch B2: stop treating menu pauses as fresh UI discovery generations

Keep this separately attributable from B1. Avoid a full observer rediscovery on
ordinary same-world menu transitions. Preserve valid class/property metadata,
refresh instance membership on appropriate lifetime/events, and bound any needed
scan. Never let stale UI objects authorize gameplay. The production verdict stays
unchanged; this is observer lifecycle/cost work, not replacement of its oracle.

Acceptance: the same pause no longer schedules the half-second scan; actual
world changes still refresh correctly. Compare time to first fresh snapshot and
stereo draw separately from weapon identity reuse. Reuse the transition lifecycle
if appropriate, but do not combine both behavior changes in the first visual A/B.

### Patch A0: event-local two-eye evidence before a world-render change

Reuse existing frame/pose/capture records, with a bounded ring captured around a
marker or gap. For each copied eye store capture serial, record/pair ID, actual
pose generation and quaternion, release identity, submitted layer/pose, timing,
and hold reason. Record left at its capture/early-return point and join both at
pair submission; store held submissions too. Missing/expired/unmatched identity
must print as unknown. A per-second right-only diagnostic is insufficient.

Join camera/render evidence to the same captured image; the current late
`render_yaw_deg` read against `delivered_rec` risks another current-versus-delayed
join. Reuse frame-id's capture-serial association where possible, and validate
that the record describes actual rendered camera behavior, not just intent.
Negative controls must perturb a diagnostic copy only: wrong eye, delayed record,
old release, missing record, and an injected missed deadline should be visible in
the expected columns without corrupting production output.

If A0 proves pose mismatch, propose pose attribution by the verified image record
with bounded missing-record fallback, preserving the tested eye geometry/FOV.
Do not merely replace a submitted quaternion: position, eye separation, neck/body
mapping, and record freshness must refer to the same rendered view. If A0 instead
shows coherent images/poses and missed intervals, optimize the measured stall
owner or test a controlled cadence/runtime setting separately. No automatic
settings changes or universal ghosting cure is selected yet.

## Exact restart checklist

1. Read this findings section and the two new report entries in FLICKER_REFERENCE.
   The source maps and preserved log already establish menu cache destruction;
   do not repeat the broad history search.
2. Before any code patch, inspect the latest HEAD/diff because another agent is
   working concurrently. Baseline and installed dirty build differ. Keep changes
   to the chosen subsystem and preserve the existing tested profile.
3. Resolve existing issue tracking for the new work (VR-77/VR-80 are related but
   not automatic matches). This pass created no ticket, branch, or PR and sent
   no messages to other collaborators.
4. Implement/test B1 first only after selecting the actual lifetime discriminator
   and lane-safe ownership protocol. B2 is next; A0 can then collect world-gap
   evidence in a separately identified build. Do not claim the present plan
   already solves the image/pose association or world identity epoch.
5. Update FLICKER_REFERENCE with every result, including failed predictions and
   unexercised tests. Preserve logs before launches. No game was launched or
   installed by this investigation; no host/render tests were needed for these
   documentation-only changes.

## Newest-log follow-up requested during investigation

Read the installed log again after the user reported another run. Saved as
`build/flicker-resume-investigation-2026-09-13/newest-run-102707.log`.
SHA-256: `DFA8FF91161484E3B52ABD92911DA2A85581E965B8D70BDACC858142C5331987`.
The available file is a longer continuation of the same startup timestamp
10047093 and build `vr33-hands-working-182-gfa2ea4e3-dirty`; do not count the two
snapshots as independent runs. Its observed size is 1,422,566 bytes and file
modification time was September 13, 10:22:45 local.

Updated latest-state GAMEPLAY gap count: 33, range 48-538 ms, including 27 in
xrEndFrame (four more than the earlier snapshot). Other gameplay owner counts
are unchanged. No V markers and no nonzero abort/stale fields in the available
`stereo: eyes` summaries. The clean post-resume mirror window at 10188406 has
15 single ticks, 15 legacy-shadow raw leaks, zero shown-right/unknown/warmup,
and zero copy failures under draw source.

A second menu transition at 10195593 drops three candidates. Contracts were
already zero, so the early return in WaInvalidateContracts produces no second
contract-drop line. Subsequent states are LOADING then MENU, not a second
confirmed resume. Contract summaries remain 0/64 after the first pause; no
re-adoption is visible. This preserves the reset finding but still does not
measure the user's perceived one-second weapon relock with a drawn weapon.

Next action remains the proposed B1 lifecycle implementation or the bounded A0
world-event instrumentation, following the restart checklist above. There is no
new evidence here that justifies changing lag, c5 arbitration, or mirror policy.

## VR-93 implementation (started 2026-09-13, branch `claude/vr-93-menu-weapon-identity`)

Status: DRAFT PLAN, written before code so the work is recoverable. Issue A
(VR-94) is not started and stays parked until B lands or is parked.

### New timing evidence (a later run, not the snapshots above)

Installed `dishonored_vr.prev.log`, build `vr33-hands-working-185-gd71968e3-dirty`.
One ordinary pause and resume with the sword drawn:

| ms | Event |
|---|---|
| 11827062 | GAMEPLAY again (view live at once after a menu's silence) |
| 11827062 -> 11827562 | `uistate: scan #2 ... in 499 ms` on the script lane; a 523 ms frame gap "waiting for the game thread"; SINGLE draws throughout |
| 11827578 | candidate list rebuilt; DOUBLE after 137 single ticks |
| 11827593 | `Wpn_PlySword01` accepted on the view model - re-adoption took **15 ms** |
| 11827921 | hand-mesh calibration done (pivots, signs) - **+343 ms** after adoption |

So the reported settle is two costs in series: about half a second of mono view
while the observer rescan stalls the game thread (B2), then about a third of a
second of relearning (B1). Adoption itself is not the slow part.

### B1 design: suspend, then validate on resume

- Lever `[Hands] AttachKeepOnMenu`, repo default 0 (today's behaviour exactly),
  live in F10 Hands. Armed in the test install only.
- Present lane, leaving GAMEPLAY: `SuBeginLoad` stays unconditional. If the lever
  is on AND the destination is MENU AND the pawn is live, record the epoch (pawn
  and controller pointer, class, FName) and SUSPEND instead of invalidating.
  Every other destination takes today's path.
- Present lane, while suspended: NO_PAWN, or a different pawn latched from the
  event stream, hard-invalidates at once with the reason logged.
- Present lane, back to GAMEPLAY while suspended: VALIDATE_PENDING (epoch++).
  A benign LOADING between MENU and GAMEPLAY does not end the suspension.
- Script lane, first tick in VALIDATE_PENDING, BEFORE the UI scan, the candidate
  rebuild and the component snapshot: rebuild the live-object table, then require
  pawn and controller unchanged (pointer, class, FName) and IsLiveObject; every
  candidate and every contract component IsLiveObject with the class and FName
  it had when collected. Any failure, or a refused table rebuild, invalidates
  everything (fail safe to today). No snapshot is published until the verdict.
- Retained identity is still not trusted per draw: every correction already
  requires a component snapshot and a hand correction from THIS present and eye,
  so nothing stale from before the menu can be consumed.
- Equipment changed during the menu is caught by the existing equipment
  revision check (retained, not reset to 0), which marks the list dirty.
- `MpOnReset` stays a hard boundary and is untouched.
- One bounded log line per resume: verdict and reason, retained counts, and
  the time from GAMEPLAY to first DOUBLE, first fresh snapshot and first
  corrected weapon draw.

Residual risk, stated: a real load that recreates the pawn, controller and every
component at the same addresses with the same classes and FNames would pass. The
FName number of a spawned actor is expected to change on a respawn; that is NOT
verified, and the negative-control launch below exists to test it.

### B2 design (separate lever, separate launch)

`[Menu] UiKeepOnMenu`, default 0. When on, a retained suspension does not queue
the observer rescan; an invalidation still does. The production gameplay verdict
is not touched.

### Launch plan (one question each)

1. B1 armed, B2 off. Pause with a weapon drawn, resume. Does the weapon flicker
   after the view returns to stereo? Log must read RETAINED.
2. B1 armed. Load a save from the pause menu. Log must read INVALIDATED with a
   reason and weapons must still lock after the load. RETAINED here means the
   discriminator failed: disarm and redesign.
3. B2 armed. Pause and resume: is the half-second hold at resume gone?

### Checkpoint

- [x] Plan written
- [x] B1 code (`hands/menu_keep.h`, `menu_keep.cpp`), `tools/menu-keep-host.ps1`
  35 checks pass and two injected faults fail them; build
  `vr33-hands-working-190-gf385bfce-dirty` installed, `AttachKeepOnMenu=1` armed,
  ini diff is that one line, CRLF intact. Pre-install logs and ini in
  `build/vr93-logs/` (ignored)
- [x] Launch 1 read: four pauses RETAINED, 0 adoptions, no weapon flicker after
  resume on the headset; the predicted ~530 ms rescan hold remains (FLICKER_REFERENCE 3.13)
- [x] B1 committed `0ff00ebb` and pushed; measurements on the ticket
- [x] Launch 2 read (build 191): the save load read INVALIDATED via the pawn
  tripwire; the pawn's FName did NOT change on respawn, so `LoadGameClicked` was
  added as a hard drop. The run ended in a pre-existing GC crash, filed as VR-96
- [x] B2 code, launch 3 read (build 192): first DOUBLE +24-26 ms on kept resumes,
  no rescan. Books read LOADING and are not covered. The book flicker was measured
  as a separate issue: while `c5` reads zero (cause not identified; dark vision
  at most one source) a re-arm's ring skew is not corrected (FLICKER_REFERENCE 3.14)
- [x] Books: `[Hands] AttachKeepOnNote` (the observer's note-movie open bit lets a
  LOADING-with-book suspend), 43 host checks; plus the read-only `[Menu] UiFlags`
  reporter for the flags in GAMEPLAY_STATE section 9. Installed and armed, build 193
  built 13:49. Launch 4: four books retained, DOUBLE +14-41 ms; the flicker after the
  fourth close is VR-80 (FLICKER_REFERENCE 3.15), not this. m_bNoteVisible verified
- [ ] FLICKER_REFERENCE, STATUS, TRAPS updated in the same commits
