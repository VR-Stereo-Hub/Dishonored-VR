# Status

## CURRENT (2026-09-09, evening): the WEAPON judder is fixed too

**`[Hands] PoseLag=2`.** Headset-confirmed by a reversing A/B/A/B. The tester's
verdict was that it fixed the weapon judder completely and the game is smooth
even with the throughput deficit still open.

Installed and known-good: 2750x2850, `VirtualMode=1`, `[Pace] Lag=2`,
`[Hands] PoseLag=2`, 80 Hz, spacewarp off.

### The fault

The engine renders a frame from the head **two locate generations back** - the
same fact that put `[Pace] Lag=2` in place for the world. `MpDriveTick`
normalised the hand against the **freshest** head, so the hand was expressed
relative to one head and planted in a view built from another. The residual is
two generations of head rotation.

`Lag=2` did not create it. It **revealed** it, by taking the world's judder away.

### How it was found, in order

1. **A motion matrix, no code.** Head rotation showed it; a stick turn did not.
   That last row is the discriminator: a stick turn moves the game camera without
   moving the head, so controller sampling and viewmodel animation are excluded.
2. **The first instrument was near-circular and returned a clean zero.** It
   compared the head stamped at the pose consume against the head the camera
   write snapshotted - both derived from the same consume. Generation gap 0 on
   every frame. The negative result is what named the correct pair: fresh versus
   **RENDERED**, not fresh versus fresh.
3. **A lag finder settled it.** Mean `|dB - dHead|` over 4085 moving frames:
   lag0 1.190, lag1 2.406, **lag2 0.119**, lag3 2.405, lag4 1.192 deg. Ten times
   clear, with a clean V around the minimum. It compares **deltas only**, because
   the camera basis is game space and the head is XR space and differencing those
   is what produced two retracted numbers here.

### Performance: measured, and two candidates falsified

Open, and tracked as VR-67. What is now known:

* **The runtime's period is not fixed and nothing could see it before.** One run
  was asked for 40 fps (25 ms), the next for 80 (12.50 ms). Every change is now
  logged; that run showed zero changes.
* **At 80 Hz the app delivers 57-80 of 80**, matching the reported 60-70 in the
  hub. GPU 7.7-12.4 ms per tick against a 12.5 ms budget.
* **The worst window is not obviously pixel-bound**: 57/s, 17.5 ms tick, GPU
  12.4 ms, but render-thread R 11.6 ms + desktop Presents 4.8 ms against a
  **0.1 ms** pacing wait. CPU/driver/synchronisation is not cleared.
* **Falsified**: the FrameId render-target readback, and D3D9Ex maximum frame
  latency at 1/2/3. Both inside the noise floor on median, tail and hitch count.
* **All gameplay hitches sit in the submission tail** (`xrEndFrame`), 19 of 19 in
  the last measured run. Where a wait is observed does not name what caused it -
  that call takes a mutex and can wait on the previous submission and on D3D11
  synchronisation, so our own GPU work can be charged to it.

`docs/dishonored/PERF_PLAN_2.md` and `PERF_REVIEW_2.md` carry the full record,
including four claims made and retracted along the way.

### Next steps

1. VR-67, the throughput deficit. Start with the 57/s window's split, not with a
   resolution change.
2. Finish the instrument repairs in `PERF_REVIEW_2.md` section 11 - the GPU span
   still subtracts an interval it does not contain, and the D3D11 bridge is
   unmeasured.
3. VR-64, the weapon swap flicker. The `wa/key:` instrument has shipped and has
   never been read.
4. VR-57 the crosshair (Urgent), VR-58, VR-56.

---

## CURRENT (2026-09-09, late): the head-turn judder is FIXED

**`[Pace] Lag=2`.** Headset-confirmed. The tester's summary was that the game
feels an order of magnitude better to play, and smooth enough that synchronous
spacewarp works well on top of it. This was the oldest and most damaging
complaint in the mod.

Merged to `VR-Main`. Installed and known-good: `vr33-hands-working-65-gd89beb93`
with `[Pace] Lag=2`, `[Stereo] LagAB=0`, 2750x2850, VirtualMode=1, 60 Hz.

### Why

The pose submitted with an eye image is chosen from a history of located views by
a fixed generation offset. The old offset of 1 was calibrated against BioShock 1's
SINGLE-THREADED renderer; this game has a separate render thread and a delayed
D3D9 capture stage, so the pixels reaching the compositor are one generation
older than that assumption and the pose described a head that had already moved.

Only a PHYSICAL turn showed it because the compositor reprojects for head motion
alone. Measured by switching it live in one run: the submitted orientation sat
0.119-1.079 deg from the sample the camera consumed under lag 1, and 0.000-0.040
deg across a full twenty seconds of lag 2 at head speeds to 106.6 deg/s, then
returned to 0.47 within two seconds of lag 1 coming back. The tester felt the
same three phases in the same order without being told which was which.

### Two wrong turns worth not repeating

**An ini key that exists beats every compiled default.** Two builds changed the
default from 1 to 2 and then to 0; the installed ini names `Lag=1`, the loader
reads a default only when the key is ABSENT, and both ran at lag 1. Their results
were read as the new value failing, and pose selection was wrongly declared
eliminated. The loader now logs the effective value and whether the ini overrode
it.

**Submit cadence was not the cause.** The tester falsified it directly: buttery
smooth at 61-72 submits/s with an inconsistent presentation rate, the same
cadence that had juddered.

### Open, and honest about it

* The camera record still does not guarantee it holds the sample the camera was
  calculated from - both writers calculate from the loose globals and consume the
  coherent sample afterwards. The signature was fixed; the callers were not.
* `g_viewsGen` is stamped one increment behind.
* The render leg - what rendering actually consumed - was never obtained; the
  world view-projection is still unidentified. `c0..c3` is uploaded ~33 times per
  view and the block sampled was not a perspective world view.

The fix stands on a measured mechanism and a reversing A-B-A, not on those.

### The resolution ask: FIXED and CONFIRMED (VR-66)

The stale command line suspected here was **ours**. The size had two homes and
one writer: `dishonored_vr_launch.txt` drives the `-ResX/-ResY` the engine
obeys, `[Screen] RenderWidth/Height` drives the mode VirtualMode advertises.
Hand-editing the ini moved only the second, so the engine asked for the launch
file's five-day-old 2750x2850, that size was no longer advertised, and UE3 fell
back to a real display mode - the fullscreen 2560x1440. Neither ini ever
contained that number.

The ini is now the authority: the ask is resolved from `[Screen]` on the
engine's first `GetCommandLine` call (outside the loader lock), one resolved ask
feeds both the command line and the advertised mode, a disagreement logs
`launch: THE TWO ASKS DISAGREED` and rewrites the file, and the hooks install
even with no launch file.

**Confirmed in a run.** 3190x3306 (the same 55:57 aspect, 10.55 MP against 7.84)
was armed and honoured end to end: `res: HONOURED`, `capture: 3190x3306`,
`xr: swapchain pair 3190x3306`, bbox 100% x 100% FULL. **The engine never had a
ceiling** - it was asking for a size nobody was advertising. 2750x2850 is
restored; the size is now a performance question, not a correctness one.

### Next steps

1. Run the VR-66 verification above at a raised size, and read the three lines.
2. VR-64, the weapon swap flicker. The `wa/key:` instrument naming which of the
   fourteen contract key fields differs on a re-match has shipped and has never
   been read.
3. Repair the three instrument defects above, so the next pose question can be
   answered by measurement rather than by an A/B.
4. VR-57 the crosshair (Urgent), VR-58, VR-56.

---

## CURRENT (2026-09-09, later): attachment fixed twice; the flicker narrowed

Branch `claude/vr-62-startup-phase-timing`, pushed, **not merged, no PR**.
`VR-Main` is still at `b38519c3`. Last headset-tested build was
`vr33-hands-working-54-gda1d760d`; the build after it is not yet tested.

### Headset-confirmed this session

| What | Result |
|---|---|
| Weapons never attached after a second load | **FIXED** - attaches instantly on every load |
| Weapon flicker in stereo | **Largely gone** - the unreadable-step instrument fell from 31 windows with bursts of 565/396/350/293 presents to one window of one present |
| Hands flickering in mono | **FIXED** - a regression this session, caused and corrected in the same session |
| Mono window after a load | 2.1-2.3 s, against 24 s when VR-62 opened |

### Falsified this session, and one number retracted

Taking the weapon eye from the drawing pass executed **zero times in 83,400
draws**. The stereo passes run on the GAME thread and the palette draws on the
RENDER thread, so there is no stack to look up. The lever stays, default OFF and
inert, as the record. **The 39% disagreement figure reported earlier came from
reading a bare global across those two threads and is retracted** - it should not
be cited. ENGINE_NOTES carries all of it.

### Not yet tested: the bolt after a weapon swap

Reported: equip the pistol, switch back to the crossbow, and the **loaded bolt**
is unattached in its default position. Everything else stays attached.

Same class of fault as the load one, one level down. The list has an owner for
EMPTY and had none for STALE. Three things were needed and only the first is
obvious - an equipment-change trigger keyed on the item OBJECT rather than its
class name, a SETTLE WINDOW because the child component need not exist in the
same tick as the equipment event, and the equipped items as COLLECTION ROOTS
because the bolt is a child of the crossbow and not of the pawn. Plus retirement
of contracts by ownership, since the existing retirement can only be reached by a
contract whose buffers are still being drawn.

New levers, all default ON: `[Hands] AttachCollectEquippedRoots`,
`AttachSwapSettleTries=5`, `AttachSwapSettleGapMs=400`.

### The residual flicker, stated precisely

> The unreadable-step fallback is substantially less active in the tested build.
> Residual flicker remains unexplained. The instrument does not independently
> verify every inferred eye.

It records *unreadable* steps, not verified wrong-eye decisions, so a step the
heuristic reads confidently and gets wrong is invisible to it. The eye inference
is NARROWED, not cleared. Uninvestigated signals already in the log: the method's
`pushed eye +1 TWICE in a row` warning, which over-claims (its predicted
`abortLeft` stayed 0 and only 4 stale-eye submits occurred all session); the tag
ring's `realigned 97 times, 3855 agree / 296 disagree`; and
`UNEVEN CADENCE: 1.18 display slots per frame` at 147-162 presents/s against
90 Hz, which `vrpace sync <hz>` can A/B and never has been.

### VR-62 itself

Untouched behaviourally. Both falsified levers stay OFF (`[Menu]
GhostClearByRate`, `[Stereo] GateOnSceneLive`). The UI probe found
`bMovieIsOpen` and remains observation-only: several movie objects read open
persistently during gameplay, so "some movie is open" is not a blocking-menu
test. The per-class census in the log is the raw material for that and has not
been analysed.

### Next steps

1. Headset-test the bolt-after-swap fix (sequence in the session notes).
2. File a Linear ticket for the bolt fault, separate from VR-62.
3. Only then, the residual flicker - starting with what it affects, not with
   another correction.

---

## CURRENT (2026-09-09): VR-62 - the mono window, three attempts falsified

**`VR-Main` is at `b38519c3`. VR-59, VR-60 and VR-61 are merged and Done.** The
weapons work: a fired bolt stays where it lands, the pistol stays on the hand at
every angle, and the mod reads equipment from the engine.

Open work is VR-62 on branch `claude/vr-62-startup-phase-timing`, **not merged,
no PR**. The installed build is `C02CB40A26477DF2`.

### The state of VR-62

A startup scoreboard exists and works. Three attempts to shorten the mono window
were each falsified in a headset, and **both behaviour changes are default OFF**;
the build behaves like the known-good one, with better logging.

| Attempt | Lever | Result |
|---|---|---|
| Clear the ghost menu flag on dispatch RECENCY | `[Menu] GhostClearByRate` | 24 s to 1.5 s, **and the main menu goes stereo** |
| Double on SCENE LIVENESS instead of the verdict | `[Stereo] GateOnSceneLive` | **hands and weapons flash behind the pause menu** |
| Force a candidate re-collect on a load | (fixed, not a lever) | partial list with no body mesh, **nothing attaches all session** |

### What is established, and should not be re-derived

1. **Nothing after the gameplay verdict holds the picture.** The verdict, the
   `[game] state: GAMEPLAY` transition and the first DOUBLE draw land in the same
   millisecond. The wait is entirely in deciding the game is in gameplay.
2. **Two of the verdict's five terms are slow by construction.** `menuOpen` is set
   by a `Dis_OpenPauseMenu` dispatch during a load when no menu is open (a
   ghost), and `viewLive` deliberately requires a full second of continuous
   dispatches to leave LOADING - measured at +1.52 s and +1.72 s.
3. **The view-dispatch rate differs by a factor of eighty** between a settling
   level (about 1/s) and a running one (about 78/s). Any dispatch-based test has
   to separate those two states.
4. **The main menu keeps dispatching view rotations** (its 3D background) and can
   have a live pawn, so neither dispatch flow nor `CylTruthLive` separates it
   from gameplay. ENGINE_NOTES 38.17 recorded this before; attempt 1 re-broke it.
5. **The camera upload serial keeps moving while a menu is up**, so "the scene is
   drawing" cannot tell a pause menu from a load.

### The rule the three failures share

Every attempt replaced a slow conservative test with a fast one. Each was right
about the slowness and wrong about the replacement, because **the fast signals do
not separate the states that matter.** The next attempt needs a signal that
distinguishes a main menu from gameplay, and a pause from a load, DIRECTLY -
not a faster version of one that cannot.

The most promising unexplored lead: `g_mainMenu` is set from named ProcessEvent
dispatches rather than inferred, so it may be a real discriminator. Read how it
is set in `ue3/process_event.cpp` before trusting it.

### What is safe and staying

* The startup scoreboard (`startup.cpp`), read-only, one line per load naming the
  term that settled LAST. It records the LAST false-to-true transition, because
  recording the first made it blame the wrong term - the clock starts as the game
  leaves gameplay, when the outgoing pawn is still alive.
* Weapon contracts are dropped when the game leaves gameplay, and a contract
  whose component has been missing for about a second is retired. Without this a
  level load left every contract pointing at a destroyed component, which is a
  LOCKOUT rather than a refusal: a refused draw returns before the matcher, so
  the contract can never be re-adopted.
* A candidate list with no body mesh is discarded and re-collected, bounded at
  120 attempts.

### PLAN FOR THE NEXT SESSION - paste it here before starting

> **This section is empty on purpose.** Drop the agreed plan in, replacing this
> quote, before any code is written. A plan that lives only in a chat is lost the
> moment the chat is, and three attempts were already spent last session on ideas
> that were sound in isolation and wrong against facts recorded further up this
> file.
>
> Whatever goes here should name, for each step: the SIGNAL it depends on, which
> two states that signal separates, and how the run would show the step failed.
> The three falsified attempts all skipped that last part.

---

### Next steps

1. Find a real main-menu discriminator, then re-try attempt 1 behind its lever.
2. VR-16, the weapon and hands flicker for the first seconds after a load - the
   tail of the same settle.
3. VR-49, the parent ticket, still carries the eye-starvation half of the settle.
4. VR-57 the crosshair (Urgent), VR-58, VR-56.

## PREVIOUS (2026-09-08, night): VR-59, VR-60 and VR-61 all confirmed in a headset

Three tickets are fixed and headset-confirmed this session. **Nothing is merged.**
Two PRs are open against `VR-Main` and its stack.

| Ticket | What a player sees | PR |
|---|---|---|
| VR-59 | a fired bolt stays where it lands, visible and solid | #23 into `VR-Main` |
| VR-61 | (no visible change) the mod can read what the game is doing | stacked |
| VR-60 | the pistol stays on the hand at every angle | stacked |

### The through-line, which is the useful part

All three were the same mistake at different depths: **the mod acted on a guess
about game state because it had no way to ask.**

* VR-59: a contract identifies a GEOMETRY and was used as an INSTANCE. 196,619 of
  196,623 corrections were made on buffer identity alone while the only test that
  checks where a draw is ran 4 times in 13 million draws.
* VR-60: the pistol had no identity at all, so it was verified against another
  weapon's component. Its 45-degree detach cone was `AttachPassRadius` converted
  into an angle by geometry - a held weapon orbits the head, so 60 uu at 45
  degrees puts the view model about 78 uu from the camera. **Any threshold would
  have produced some angle, because the identity was what was wrong.**
* VR-61 is the general answer: ask the engine.

### VR-61, and the research failure worth keeping

**This repo already had a property resolver and a session reinvented it.**
`FindPropOffset` / `FindBoolProp` in `ue3/uobject.cpp` have resolved properties by
name since 38.x and are load-bearing in four modules. The mistake was going to
another project for a technique before grepping here for prior art.

The existing design is also the better one and it stays: every UProperty is itself
a UObject whose Outer is the declaring class, so a GObjects scan finds it and
nothing has to be derived - no chain offsets, no candidate layout, no search to
validate. 578 lines of derivation were deleted.

What was actually missing was two small things, and both are now in
`ue3/reflect.cpp`: a CACHE (each lookup is a full GObjects scan, and the trilogy
mod measured a name scan on a cadence stuttering that game at 2-3 Hz) and a TArray
READER (the capability VR-60 needed).

### What the engine now tells us, measured across sheathe and swap

```
Primary   DishonoredWepSword   EQUIPPED   (every time weapons are out)
Secondary DisWepCrossbow  <->  DishonoredWepPistol
sheathe:   Secondary -> none, then Primary -> none
unsheathe: both return together
```

A flag that reads the same in every state is not evidence it is the right flag, so
it was identified by making it MOVE. Two corrections came out of that:

1. **`PawnInventorySlot.m_RequiredUsage` is a constraint on what may occupy a
   slot, not what is in the hand.** Reading it reported an empty item in both
   hands while the player was visibly holding a sword.
2. **`DishonoredInventoryItem` carries `m_EquipUsage` and `m_CurSocket`.** The item
   answers for itself, which is the equipped-versus-holstered distinction VR-59
   attempt 1 needed and could not get from component presence.

Also recorded as an observation and NOT a conclusion: sheathing reads socket
`none`, never `holstered`, so nothing should assume a sheathed weapon is Holstered.

### Branch stack, which matters for merge order

```
VR-Main
  claude/vr-59-fired-bolt-instance-identity   PR #23   (Fixes VR-59)
    claude/vr-60-pistol-not-in-snapshot       ancestor only, no PR
      claude/vr-61-property-resolver          PR       (Ref VR-61, also fixes VR-60)
```

The VR-60 branch holds only analysis docs and is an ancestor of the VR-61 branch;
VR-60's fix is a commit on the VR-61 branch, because it depends on VR-61's reader
and the two were verified in one run. **Merge #23 first**, then the stacked PR
retargets to `VR-Main` and closes VR-60 and VR-61.

### Next steps

1. Review and merge #23, then the stacked PR. The merge is the gate and it is the
   user's call.
2. **The arms during takedowns and chokes**, now unblocked:
   `eDisPlayerActionUsage_Fullbody` is the discriminator, and `GAMEPLAY_STATE.md`
   section 2 lists the rest of the wanted flags.
3. **VR-49, the 20-90 s settle** (Urgent). The asset-to-hand decision is now
   readable from the engine rather than inferred, which may shorten it.
4. **VR-57, the crosshair** (Urgent). Still head-locked. One ray.
5. **VR-58**, **VR-56**.

## PREVIOUS (2026-09-08, night): VR-61, the UE3 property resolver, first run pending

VR-59 is fixed and headset-confirmed; PR #23 is open against `VR-Main` and NOT
merged. VR-60 (the pistol) is blocked on VR-61 by choice, because reading the
inventory is the clean fix and a second heuristic is not.

Branch `claude/vr-61-property-resolver`. Built, installed, 88 host cases, lint
clean. **The derivation has never run against the game.**

### What this is

`src/game/dishonored/ue3/reflect.cpp` resolves a property BY NAME to its byte
offset on this build, by walking the UClass property chain. The argument for it is
`docs/dishonored/GAMEPLAY_STATE.md`: almost every hard bug in this mod came from
acting on a guess about game state because there was no way to ask.

**Only four slots were unknown** - `UField::Next`, `UStruct::SuperStruct`,
`UStruct::Children`, `UProperty::Offset` - because `kNameOff` / `kClassOff` /
`kOuterOff` are already derived on this build. The BioShock trilogy work had to
derive those first and called it the hard part.

### The oracle, which is what makes this not a guess

`kWaComponentLocalToWorld` (0x60) and `kWaComponentTranslation` (0x90) are
measured on this build and read every frame, and UE3 names those properties
`LocalToWorld` and `Translation`. **A layout is accepted only if it resolves both
names to both offsets.** A wrong layout cannot reproduce an answer we already
know, so the search cannot quietly settle on one.

If derivation fails, the log says which stage and with what numbers - including
the case where the constants themselves are wrong for that class, which would
make the search impossible and the constants the bug.

### Three traps taken from the trilogy mod rather than rediscovered

1. **NEVER init-driven.** Our DLL loads from `DllMain` during import resolution,
   before the exe's CRT static initializers, so GNames is empty then. Deriving at
   startup fails every boot and looks like a broken instrument. This derives
   lazily on the script lane and retries every 2 s until it succeeds.
2. **A name-pool scan is hundreds of milliseconds and must never be on a
   cadence** - it stuttered that game at 2-3 Hz. Every resolve here is cached per
   (class, name) for the process lifetime.
3. **`ObjectArchetype` is class-classed and SHARED**, which falsified it as a
   chain link there: two nodes cannot share a `Next`. It is excluded explicitly.

A fourth is ours: the four-way search was ~810k candidate layouts, each doing two
chain walks. On the game thread that is a hang measured in minutes, and **a
diagnostic that freezes the game is not a diagnostic.** The slots are separable,
so derivation is staged into ~1,000 walks; the oracle still judges the finished
layout, so staging changed the cost and not the standard of proof.

### The first consumer exists to make the resolver falsifiable

A resolver that derives a layout and reads nothing has proved only that it did not
crash. So it reads the one thing the component walk provably cannot: pawn ->
`m_pInventory` -> `m_Slots`, a TArray of `PawnInventorySlot`, each slot carrying
the item AND its `EDisEquipUsage`. Every step resolved by name. Logs CHANGES only.

The struct STRIDE is the single unresolved number in that path, because element
layout is not in the property chain. It is validated rather than trusted: a stride
that yields no readable item pointer is reported as **a wrong stride, not an empty
inventory** - two things that look identical without that line.

### What the next run has to answer

Read in this order:

1. `rfl: UE3 property layout DERIVED ...` with the four slots and the oracle it
   was validated against. If instead `rfl: not derived yet - <reason>` repeats,
   the reason names the stage.
2. `rfl/state: equipment CHANGED - Primary ... Secondary ...` on every weapon
   swap. **That line is the proof**: it is data out of a TArray, which is what
   VR-60 needs and what the pointer walk cannot reach.
3. Nothing should look or feel different. This subsystem is read-only, off the
   frame path, and touches no render lever.

`[Hands] StateFlags=0` turns the reader off; the resolver itself has no lever
because nothing consumes it yet.

## PREVIOUS (2026-09-08, night): VR-59 is FIXED and headset-confirmed

**A fired bolt stays where it lands.** Confirmed in a headset: bolts are visible
and solid, hold their position and rotation, show no coupling to the hand in any
weapon, and newly fired bolts behave correctly too. The held bolt and the
crossbow are unaffected. PR open against `VR-Main`.

### What fixed it, in one sentence

A contract identifies a GEOMETRY and was being used as an INSTANCE. Every draw on
a weapon's buffers is now verified against the component that contract was
matched to, using the engine's own transform from the live snapshot, and a draw
that matches nothing is handed back exactly as the engine drew it.

The measurement that proved the architecture was the problem: `known-buffer
passes 196955, corrected 196619, matched 4`. Ninety-nine point eight percent of
corrections were made on buffer identity alone, while the only test that checks
where a draw is ran four times in 13 million draws.

### The two lessons, both paid for by a headset run

**A reference has to be maintained on the path that uses it.** `lastL2W` is
written only where the transform matcher adopts a contract, so a gate built on it
compares against something that is almost always stale. It refused 53,238 held
draws in one run with the present gap growing to 21,367.

**Refusing to correct a draw and refusing to draw it are different operations,
and the second one deletes the object.** Dropping claims a draw duplicates
geometry rendered correctly elsewhere - true of another pass of the held weapon,
false of a world instance. The rule was written into `may_suppress` and then not
applied at two of the four exit paths, which cost a run where the bolts were
invisible rather than misplaced.

### VR-60 is next, and its symptom CHANGED for the better

The pistol used to turn invisible when aimed away from where a bolt was. It now
stays visible and instead detaches to its default position with its own idle
animation, reattaching when the aim comes back into range. That is this fix
working: the draw was being dropped and is now handed back, so the failure mode
went from deletion to falling back on the engine.

The cause is unchanged and is VR-60's job: the pistol is not in the component
snapshot at all, so it has no member candidate, reaches a contract only through
the buffer lookup's `vb || ib` OR, and is therefore verified against ANOTHER
asset's component. Past the radius from the bolt, it is correctly refused - the
verification is right and the identity it is given is wrong.

Branch `claude/vr-60-pistol-not-in-snapshot`, off the VR-59 branch because the
fail-safe behaviour it builds on is not merged yet.

## PREVIOUS (2026-09-08, night): VR-59 attempt 2b - refusing is not deleting

Attempt 2 verified correctly and then DELETED what it refused: fired bolts were
invisible for a whole run while the held bolt and the crossbow were fine. Two
places consumed a refused draw, and both are closed. Built, installed, 88 host
cases. Not yet in a headset.

### The lesson, which is worth more than the fix

**Refusing to correct a draw and refusing to draw it are different operations,
and the second one deletes the object.** Dropping a draw is a CLAIM: that this
draw duplicates geometry the frame renders correctly elsewhere. That is true of
another pass of the held weapon and false of a world instance, which is the only
copy of itself there is.

The rule was written into `may_suppress` in attempt 2 and then not applied at
either site that needed it:

1. **`AttachDropUncorrected` sat past the verification block** and consumed any
   draw with no correction. Verification refused the bolt correctly, and this
   line ate it two branches later. Releasing `onWeaponBuffers` did not help -
   that only guards the SECOND suppressor, out in `WaDraw`.
2. **`instVerdict` defaulted to `HELD`.** A draw whose geometry does not match the
   contract exactly never reaches verification at all - a different range in a
   shared buffer, which is exactly what a fired bolt and the pistol produce - so
   it arrived at the drop path carrying a default that said "this is the held
   item". Unverified now means unverified, and only with the lever off does it
   mean held.

A guarantee that is stated in a pure helper is not a guarantee until every exit
path is routed through it. There were four such paths and two were missed.

### The new counter that would have caught it in one run

`wa: handed back to the engine N draw(s) rather than dropped (dropped-as-
duplicate M)`, and it says on the line: **if handed-back is 0 while fired bolts
are invisible, a refused draw is still being consumed somewhere.** That is the
reading the last two runs needed and did not have.

### Still open, and expected to persist

**VR-60**: the pistol turning invisible when aimed away from where a bolt was.
It is not in the component snapshot at all, so it has no member candidate and
reaches contracts only through the buffer lookup's `vb || ib` OR - its visibility
is decided by a distance test belonging to another asset. Attempt 2b should stop
it being DELETED (an unverified draw is now handed back), but the pistol still has
no attachment of its own and that is VR-60's job.

## PREVIOUS (2026-09-08, night): VR-59 attempt 2 - verify every draw

`VR-Main` is pushed at `555e8ff4`, PRs 18-22 closed. Attempt 2 of VR-59 is built,
installed and covered by 86 host cases; **not yet in a headset.**

### The measurement that settles the architecture

From the attempt-1 run: **`known-buffer passes 196955, corrected 196619`, and
`matched 4`.** 99.8% of all corrections were made on buffer identity alone, while
the transform matcher - the only thing that checks WHERE a draw is - adopted four
contracts in 13 million draws.

**A contract identifies a GEOMETRY and was being used as an INSTANCE.** That one
sentence explains every symptom reported, and they are all one bug:

* a fired bolt rotates in place - it inherits the held bolt's delta, conjugated
  about its own origin;
* two fired bolts rotate together, each about its own origin - same delta, same
  contract;
* after a weapon switch every bolt orbits the muzzle at whatever radius it had
  when the switch happened - the delta becomes a fixed transform relative to the
  hand;
* each bolt vanishes past an angle - beyond `AttachPassRadius` the draw is
  refused, falls through to a matcher that cannot place it, and is dropped.

And the reference that gate compares against, `lastL2W`, is written ONLY where
the matcher adopts a contract - so it was current 4 times all run. A gate on a
reference nothing maintains both misfires and misses.

### The rule that replaces it

**A draw is the held item only if it is where the engine says the held item is. A
draw that matches nothing is handed back exactly as the engine drew it.**

It names no asset, no weapon and no count, which is the point - a throwable that
does not exist yet is covered without new code. Per draw:

1. The contract carries the COMPONENT it was matched to (`compObj`), not just its
   buffers. A second instance of that mesh can never inherit its correction.
2. `WaVerifyDraw` compares the draw against that component's transform from the
   live snapshot, expressed in the draw's space through the bridge the matcher
   already builds. The snapshot is engine-read and republished twice a frame, so
   it cannot go stale while the weapon is in view.
3. A recent-verification cache (`heldAt`, refreshed on EVERY verified draw by both
   routes) answers when no correction was published this Present, so a quiet
   frame does not blink the weapons.
4. Nothing to compare against means REFUSE. Absence of evidence is not consent -
   that was the 41.x defect exactly.

**Suppression is now only for a draw that PASSED verification.** That is what
keeps a world instance visible: its colour and lighting passes are its own, not
duplicates of anything we drew, and suppressing them is what made fired bolts
vanish. The shared `dm` delta is also gated on the verdict, so a refusal is no
longer advisory.

The tolerance is `AttachPassRadius` reused, not a new number: the census measured
0.3 uu between an uncorrected pass and its corrected twin, and two instances of a
mesh are hundreds of units apart. Both bounds are asserted in the host suite.

### Levers

`AttachVerifyInstance=1` (OFF restores trusting buffer identity - the behaviour
of every previous build, so the two compare directly), `AttachHeldMaxPresents=2`.
The two falsified attempt-1 gates stay at OFF, kept so their measurement is
reproducible.

### What a headset run has to answer

`wa: instance verify` reports held / elsewhere / unverifiable, which reference
answered, and the worst offset accepted next to the farthest refused - so the
radius can be judged from both sides rather than argued. **HELD should be the
large majority while a weapon is out; 0 with flat weapons means verification is
failing, not idle.** If `component` is 0 the engine-read route is not running and
only the cache is holding it up, which would be a latent failure.

The risk to watch is the opposite of the old one: too much refusal. If a held
weapon goes flat or blinks, `AttachHeldMaxPresents` and the radius are the levers,
and `AttachVerifyInstance=0` returns to the old behaviour.

Also open: **VR-60**, the pistol is not in the component snapshot at all, so it
has no member candidate and reaches contracts only through the buffer lookup's
`vb || ib` OR. That is why its visibility depended on aim angle.

## PREVIOUS (2026-09-08, night): VR-33 merged; VR-59 attempt 1 FALSIFIED

`VR-Main` is pushed and at `555e8ff4`; PRs 18-22 are closed and their tickets
are Done. **The VR-59 fix was tried in a headset and both of its gates were
falsified.** Both are now default OFF, the build is rebuilt and installed, and
the run produced exactly the measurements needed to design the real fix.

### What the run said

The held bolt stopped following the hand and only inherited camera rotation - it
was drawing natively. The pistol vanished depending on the angle between where
it pointed and where the bolt had been. No bolt was fired at all, so every
symptom was on HELD geometry: a straight regression.

**`AttachRequireFreshRef` starves the held weapons.** 53,238 refusals in one
run, all on held `crossbow_01` and `bolt_01`, with the present gap growing
monotonically to 21,367. `lastL2W` is written ONLY where the transform matcher
adopts a contract; the buffer-identity route that actually corrects the auxiliary
passes never refreshes it. Once the matcher misses, the reference is stale
forever and the gate blocks the only remaining route. **A reference has to be
maintained on the path that uses it.**

**`AttachRequireLiveMember` cannot answer its own question.** All 19 published
snapshots in the run were identical - the same six components, including
`bolt_01 (pArrowMesh_HighRes)` - across crossbow, sword and pistol being held in
turn. `FpCollect` walks the pawn INVENTORY, and `DisWepCrossbow` says why: the
loaded bolt is `m_pArrowMesh_HighRes`, a component of the WEAPON, which stays in
inventory when stowed. Presence is not equipment.

### What the scripts gave, and why it matters

In `docs/dishonored/ENGINE_NOTES.md`. The loaded bolt and a fired bolt share
**only** the mesh asset `bolt_01`; the component name, the component class and
the owning actor all differ, and a fired bolt is a separate ACTOR
(`DisProjectile_Arrow`) that never reaches the snapshot. That is why every
asset-name and buffer-identity route can be fooled, and it is the shape any real
fix has to take.

The pistol is not in the snapshot at all, which is a separate finding worth
acting on: it has no member candidate and can only reach a contract through the
vertex-OR-index-buffer match, which is what made its visibility depend on aim
angle.

### The next attempt, designed but NOT written

Compare each draw against the contract's COMPONENT position from the engine
snapshot, expressed in draw space through the bridge the matcher already builds -
not against `lastL2W`. The snapshot is engine-read and refreshed every 4 ms
whether or not the matcher succeeded, so it cannot go stale the way `lastL2W`
does. Store the component pointer in the contract at adoption so the right
component is looked up each frame.

What is kept from attempt 1: the `held_instance` predicate and its 13 host cases,
`AttachVetoReleasesBuffers` (sound - a vetoed draw must not be suppressed), and
`AttachInstanceVetoRelaxed`.

### Previous entry for this session (the merge, still accurate)

## PREVIOUS (2026-09-08, later): VR-33 merged, VR-59 attempt 1 written

Two things happened this session. **PRs 18-22 are merged into `VR-Main` locally
and are NOT pushed yet** - the push was blocked by a tool permission, so the
remote `VR-Main` is still at `f44f4761` and all five PRs are still open. The
first job of the next session is that one command, or to say so plainly if it is
still refused.

Then VR-59, the fired bolt, which is written, built, installed and covered by
host tests but **has not been in a headset**.

### The merge, and why it is five commits and not one

PR 22 turned out to be a strict SUPERSET of 18, 19, 20 and 21: it was rebuilt off
`VR-Main` and carries every feature from all four, plus later tuning that
supersedes theirs (`HeightOffsetM` -0.090 -> 0.060, `PosTrack Scale` 98 -> 108).
Merging 22 alone would have landed everything but left the other four PRs unable
to close themselves, so they were merged in order 18 -> 19 -> 20 -> 21 -> 22
instead, each as its own merge commit.

Every conflict on the way to 21 was two branches appending to the same region - a
new `CURRENT` section, a decision-log entry, `#include` lines in the unity TU -
and was resolved as a UNION, so each feature keeps its own section. The final
merge took 22's side throughout, because 22 is the integrated superset, and then
22's own cleanup was applied (the 23 scaffolding docs it replaced with one
durable record, and two experiments it retired to `src/legacy/vr33/`, which is
why nothing was lost).

**The end state was verified by construction: the merged tree is byte-identical
to PR 22's tree**, which is the headset-confirmed build. `git diff HEAD
origin/claude/vr-33-rotation-grip-and-weapons` is empty. Lint clean, Release
builds, nine exports undecorated.

### VR-59: a distance can never answer an instance question

The branch is `claude/vr-59-fired-bolt-instance-identity` off the merged
`VR-Main`. **No PR** - deliberately, on request.

The three radius gates could not close this and were never going to. A bolt fired
into a surface a metre away is inside all of them on merit. What identified the
real defect was that the fault behaves COMPLETELY DIFFERENTLY depending on what
is held: with the crossbow out a fired bolt only inherits its rotation, but with
the pistol out the bolt jumps onto the aim direction and tracks the pistol - **at
any distance, near or far.** A fault that reaches an arbitrarily distant instance
proves no radius was gating it.

**A contract outlives its weapon being stowed, and the instance gate ran only
when a fresh reference existed.** `g_waMesh` is keyed on buffers and evicted only
when the table fills. The pass-radius check ran behind
`if (lastL2WOk && present - lastL2WPresent <= 2)`. Stow the crossbow, the loaded
bolt stops drawing, that reference goes stale, and **the gate is skipped
entirely** - while the hand that contract belongs to keeps publishing a fresh
correction every frame, because the hand is always drawn. The absence of evidence
was being read as permission.

The dark stub left standing where the bolt landed is the same cause, not a second
bug: some passes were corrected and the rest were SUPPRESSED by
`AttachSuppressUnplaced`, whose documented cost is exactly this when it lands on
a world instance - those colour and lighting passes are the bolt's own, not
duplicates of anything we drew.

### What replaced them

`dvr::wf::held_instance` in `weapon_frame.h`, pure and fully exercised by
`frame_test`. Four verdicts, and the ORDER is the authority they carry:

* `STOWED` - that asset is not a live member of that hand in the current
  component snapshot. **Strong, and it is the engine's own answer**: `FpCollect`
  walks out from the pawn through the inventory chain only, so a world projectile
  cannot appear in it however close to the camera it sits.
* `ELSEWHERE` - a fresh reference exists and this draw is past
  `AttachPassRadius` from it. Strong.
* `NO_REF` - nothing has vouched for this geometry for `AttachRefMaxPresents`
  presents. **Weak on purpose.** It refuses the correction, but it may not
  release buffers or overrule the relaxed band, because a weapon just re-equipped
  has a stale contract by definition - treating it as strong would stop a
  re-equipped sword relocking and would draw a ghost copy while it tried.
* `HELD` - corrects.

Five levers, `[Hands]`, **all default ON**: `AttachRequireLiveMember`,
`AttachRequireFreshRef`, `AttachRefMaxPresents=2`, `AttachVetoReleasesBuffers`,
`AttachInstanceVetoRelaxed`. Each one off restores the pre-VR-59 behaviour of
that single step, so they A/B alone; the host suite asserts that too.

### Verified on the desk

75 host cases pass (13 new, all on the instance verdict), `tools\lint.ps1` clean,
Release built and installed, installed DLL hash matches the build. The installed
ini has `AttachWeapons=1` and `AttachSnapshotMaxMs=100` and none of the five new
keys, so all five take their ON defaults.

The new cases deliberately hold the DISTANCE inside the radius and vary only the
instance evidence - a suite that separated them by distance would be testing the
gate that already failed.

### Next steps

1. **Push `VR-Main`** (`git push origin VR-Main`), then confirm PRs 18-22 closed
   and VR-30, VR-31, VR-33, VR-51, VR-53 moved to Done.
2. **A headset run for VR-59.** Fire a bolt into a wall a metre away, look at it,
   then switch weapons and look again. `wa: instance gates` reports every
   counter; the line says on itself that ALL ZERO IS THE HEALTHY READING while a
   weapon is held and drawing, because it counts draws that are not the held
   instance. A non-zero `stowed` count with the bolt sitting still is the fix
   working.
3. **Watch for the regression this could cause**: re-equipping a weapon must
   still relock, and must not show a ghost copy while it does. That is what
   `NO_REF` being weak protects, and it is the one thing in this change that
   trades against the fix.
4. **VR-49, the 20-90 s settle** (Urgent). The weapon lock is part of it and the
   decisions are cacheable.
5. **VR-57, the crosshair** (Urgent). Still head-locked while the weapon points
   where the hand points. One ray.
6. **VR-58** (numpad adjust, ModelScale) and **VR-56** (no back faces on the
   weapon models).

---

## PREVIOUS CURRENT (2026-09-08): VR-33 is DONE and in review

The hands and the held weapons are on the tracked controllers, headset-confirmed
and stable. The branch is `claude/vr-33-rotation-grip-and-weapons`, twelve
commits, and the PR is open against `VR-Main`. **Nothing here is merged.**

### What a player sees now

* Hands track position and rotation, and hold still when the head moves.
* The crossbow, the loaded bolt and the sword follow their controllers, keeping
  the game's own hand-to-weapon and weapon-to-bolt relationships and their
  internal animation.
* No duplicate weapon standing at the position the engine drew it.
* A rare single-frame blink on the weapons, and a fired bolt close to the player
  can still be picked up. Both are ticketed, neither reads as broken.

### The three things worth knowing before touching this

**The grip transform is a REFLECTION.** The draw's camera basis is right-handed
and the engine is left-handed, so the pose mapping is a mirror. Three Euler
angles cannot carry one; the saved record stores a parity sign beside a proper
rotation and refuses a pre-version record rather than loading a hand inside out.
A build that demands a proper rotation refuses every draw.

**A weapon mesh is drawn by several passes, and any pass we do not place draws
itself at the native position.** That is what the duplicate copies were. The
contract key must include the VERTEX SHADER; without it an uncorrected pass
looks like the contract's own draw.

**Suppressing a pass is only safe when it would otherwise draw a second copy.**
Several of a weapon's passes are its colour and lighting contributions.
Suppressing those leaves the ambient term alone: a translucent weapon that
vanishes in shadow. That was measured, not guessed.

### The blink, and the shared gate behind it

The two weapons share no contract, buffers, component or match - only their
inputs. **They blink together, which is what identified the cause**: a shared
gate, not two independent failures. The gate was the component-snapshot
freshness bound, tightened from 100 ms to 20 ms while chasing a view-model sway
theory the headset then falsified. The theory was dropped and the bound was not.
Back at 100 ms the blink is rare.

`AttachSnapshotMaxMs` is the lever. **Tightening it blinks both weapons.**

### Next steps

1. **Merge VR-33** when the reviewer is satisfied - the user's call, not an
   agent's.
2. **VR-59, the fired bolt** (High). A bolt fired into something close by is
   inside every distance gate and is the same mesh drawn from the same buffers.
   The gates cannot close it; the fix is an instance identity read from the
   engine. This is the next session's focus.
3. **VR-49, the 20-90 s settle** (Urgent). The weapon lock is now part of it.
   The asset-to-hand and asset-to-space decisions do not change between runs
   even though the buffers do, so they are cacheable.
4. **VR-57, the crosshair** (Urgent). It is still head-locked while the weapon
   points where the hand points. One ray.
5. **VR-58**, the numpad adjust and ModelScale, neither ever exercised in a
   headset. Both default to the identity, so the build that was tested is the
   build that ships.
6. **VR-56**, the weapon models have no back faces. The asset was authored to be
   seen from one side.

### Verified on the desk

49 host cases (28 hand, 21 weapon), `tools\lint.ps1` clean, nine exports
undecorated, the installed DLL matching the build. The simulator was not used
for this work: every question on it was perceptual.

### The record

`docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` is the one durable document -
mechanism, levers, and a graveyard of every approach that cost a headset run.
Section 8 is worth reading before any similar work: five separate filters in
this investigation each produced a confident zero while excluding the thing they
were built to find, and an enumeration solved in one run what they had hidden
for eight builds.

## PREVIOUS CURRENT (2026-09-08): VR-33 weapon attachment direct repair, test pending

Release fix installed; 28 hand tests and 21 weapon tests pass. The first headset run confirmed weapon
motion, but wrong hands, a large offset and flickering old-position silhouettes.
The follow-up corrects hand settings and permits qualified non-camera passes.
Correct alignment and ghost removal remain pending the next headset run.

No sweep or blinking is expected. Equip crossbow/sword, move each controller,
then check the loaded bolt, head motion, both eyes and re-equip/reload.
Read `wa: beat v2`, `wa: interval nearest`, and per-asset `wa: contract` counts.
Sword defaults RIGHT (1); crossbow/bolt LEFT (0). Hand calibration is retained.

Full implementation and remaining assumptions:
[VR-33 direct fix handoff](dishonored/VR-33-HANDS-AND-WEAPONS.md).

The old sweep-based test instructions below are historical and superseded.

## Historical (2026-09-07, night): VR-33 - weapon attach armed, model scale added

### TEST TOMORROW, in this order

**1. THE WEAPONS (priority).** Launch, draw the crossbow, stand still somewhere
quiet facing a wall, let the sweep run (~26 s of blinking), then keep playing.
Nothing to press.

* If it worked: the weapons follow your hands, and the log has
  `wa: ADOPTED 'crossbow_01' ...` followed by `wa: ... ATTACHED`.
* If it did not: the log now says WHY. Read these two tables, in this order:
  * `wid: draws per phase` - total draws in each phase. **If the total does
    not fall when a component is hidden, its draws are still not in the
    population** and nothing below it means anything.
  * `wid: the 16 largest baseline signatures` - each buffer pair's whole phase
    vector. A weapon's pair should read high, 0, high, 0, high across its own
    four phases. This separates "drops on one hide only" from "never drops at
    all", which have looked identical for three runs.

**2. THE SIZE, once the weapons are settled.** F10 -> **"hand / weapon size"**,
or `[Hands] ModelScale` in the ini. Try **0.75**. It scales the hands AND
anything held in them by one factor, about the tracked palm.

* EXPECTED: hands and weapon shrink together, the grip stays put, the world
  does not change size.
* Ships at **1.00**, which is exactly the identity it replaces - so test 1 is
  measured on the same geometry every previous build was.
* Watch for hand shading getting brighter or darker as you move the slider:
  two of the three hand shaders push normals through these palette rows, and
  if they do not renormalise, a uniform scale changes normal LENGTH. Direction
  is safe either way.
* Also watch the wrist cap for a hole opening at the cut.

### Where the weapon work actually stands

The sweep is CORRECT and has been for three runs: all 16 phases, every hide
and restore verified against `HiddenMaterials`, and the tester watched the
sword, crossbow and bolt each blink twice on command. What kept failing was
never the sweep - it was what the sweep could SEE.

Three faults found and fixed, in order:

1. **The report was lost to a lane that stopped** (`5df74130`). `WiTick` runs
   on the script lane; the last phase ends on a deadline, so the report needed
   one more tick of that lane, and the lane went quiet half a second later. 26
   seconds of correct measurement, no output. The terminal step now runs from
   whichever lane reaches the deadline first, behind an interlock.
2. **The signature was too fine** (`c1fc4c55`). It hashed the draw's own range,
   so a mesh drawn from a shared buffer became many short-lived signatures. Now
   keyed on the vertex+index buffer pair - the key the mesh lock already uses,
   and the one that names a GEOMETRY.
3. **The recorder never saw the weapons at all** (`bc73daed`). It sat behind
   the palette gate (a fresh c6 upload within the reuse window) - right for the
   census, wrong for identification. `vs_const_hook.cpp` already describes the
   crossbow's body as a STATIC attachment, which that gate excludes. It now
   sees every indexed draw and records whether a fresh palette was pending.

**The evidence that named fault 3 was in the log for two runs and was not
read**: reject counts of 146/154/151/153, then 51/50/51/48 - near-identical for
every component INCLUDING the player body. Counts that uniform are not a fact
about components. That is what "the thing being measured was never in the
population" looks like, and it survived a change of signature key because the
key was never the fault. Hence the two new tables above: a third failure
cannot now be silent.

### The attachment itself

`weapon_attach.cpp` (+ state chunk 57b). The sweep's owned buffer pairs are
handed straight to placement - identification is the INPUT to attachment, not
a report somebody reads and types back. Those draws then take the same rigid
palette correction the hands take, from the same `g_mpPalmTarget`.

A weapon assembly is rigid, so its frame is the palette's first bone rather
than an averaged anchor; every bone gets one common transform, which is what
keeps the loaded bolt animating with the stock instead of being pinned
separately. A mesh with no palette is treated as one bone, three registers.
c6 is current device state, so the game's own block goes back after the draw.

Fail soft throughout: no target palm, an unreadable palette, a frame that will
not normalise or a non-finite result all draw the engine's own weapon and log
which. Blast radius is bounded to buffer pairs the sweep proved, so the
confirmed hand path cannot be reached. `AttachWeapons=0` removes it.

### The model scale (`21d3e3eb`)

PageUp/PageDown were never a size knob. `g_posScaleUU` sets the stereo
separation - a property of the PROJECTION - so it resizes the whole frame at
once. The hands were not scaling with the world, they were scaling because of
it, and that knob could never have made them smaller relative to the room.
Hand TRAVEL is already independent (`WorldScaleUU`, `PaletteDriveGain`), which
is why tracking felt right while the models were too big.

`[Hands] ModelScale` is a uniform factor about the target palm, so the grip
stays where tracking put it. The weapon path takes the same factor about the
same palm, so the two cannot drift apart. `delta_from_target` now hands back
the palm in local space (it always computed it and threw it away).

The F10 "hand / weapon size" slider drives this instead of `HandSize`.
`HandSize` wrote `SkelControlBase.BoneScale` engine-side, which only reaches
SkelControl-driven bones and could never resize a separately-componented
crossbow. The key still loads for the legacy drive; config warns with the
product if both are off 1.0.

### The levers as installed

`AttachWeapons=1 AttachSwordHand=0 AttachCrossbowHand=1 WeaponId=1
WeaponIdMs=1500 MatCycle=0 PaletteRotate=1 ModelScale=1.00 HandSize=1.00
Adjust=1 AdjStepT=1 AdjStepR=3`, per-hand trim under `TrimLTX..TrimRRZ`, grip
calibration under `GripL*`/`GripR*`. `MatCycle` must stay 0 - the identifier
drives the same hide/restore calls and refuses while the cycler is armed.

### Still untested from the previous session

**The numpad hand adjust has never been pressed in a headset.** Numpad 9
cycles LEFT position / LEFT rotation / RIGHT position / RIGHT rotation and
names the mode; 8/2 forward/back or pitch, 6/4 right/left or yaw, 0/5 up/down
or roll; 7 cycles the step. Every press logs and saves. The census gives up
Numpad 4-9 while `Adjust=1`, the cycler gives up 2, and the mesh split's mode
cycle moves from Numpad 0 to Numpad 1 - the startup log names all of it.

**The grip restart path is confirmed** only insofar as the calibration loads;
the hands were reported correct on the runs since.

### Verified on the desk, not in the headset

28 frame-maths cases pass (`build\src\RelWithDebInfo\frame_test.exe`),
including the new `model_scale_about_the_palm`, which FAILED on its first run
and was right to - it measured distances from `D(palm)` when the palm is a
point in the OUTPUT space that `D` maps the source anchor onto. `lint` clean,
exports clean. **The attachment and the model scale have never been in a
headset.**

### Next steps

1. The two tests above.
2. If the weapons attach, the remaining W-items are in
   `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md`: re-acquire on equip
   change (buffer pointers can differ after a re-equip), and ownership during
   reload and release.
3. `pcap/layout` was printing at draw rate and produced a 25 MB log in one
   short run; now once per shader. Worth a look for other unbounded per-draw
   lines in the same family.

## PREVIOUS CURRENT (2026-09-07, later): VR-33 - the numpad adjust and a weapon identifier that can fail

### WHAT TO TEST, in order. Everything is installed and armed; just launch.

The build is Release, installed, and the ini is set. Nothing needs typing and
no key needs pressing to arm anything.

**Test 1 - the calibration survives a restart.** Launch and look at your hands
before touching any key. The grip was solved and saved last session
(`GripLVersion=2 GripLParity=-1`, `GripRVersion=2 GripRParity=-1`).

* EXPECTED: the hands are at the right angle immediately, the same as they
  were after SHIFT+F7 last time. The log says
  `config: the <side> hand's grip calibration LOADED - version 2, parity -1`
  for both hands, and NOT the "NO CALIBRATION" warning.
* IF THEY ARE MIRRORED OR INSIDE OUT: the saved record is not being applied.
  Say so; the log line above is the one that matters.
* IF THEY ARE AT A WRONG ANGLE BUT NOT MIRRORED: the record loaded but is
  wrong. SHIFT+F7 will re-solve it.

**Test 2 - the numpad adjust.** Press **Numpad 9** four times, slowly.

* EXPECTED: four log lines naming `LEFT hand POSITION`, `LEFT hand ROTATION`,
  `RIGHT hand POSITION`, `RIGHT hand ROTATION` in that order, each printing
  that hand's current trim.
* Then in `LEFT hand POSITION`, press **Numpad 8** a few times. EXPECTED: the
  LEFT hand moves along its own fingers, 2 cm per press, and the right hand
  does not move at all. Numpad 6/4 move it across the palm, 0/5 out of it.
* **Numpad 7** cycles the step (0.5 / 2 / 5 cm). In a rotation mode it cycles
  0.1 / 0.25 / 0.5 / 1 / 2 / 5 / 15 degrees.
* Every press writes to the ini, so stop wherever it looks right and it will
  be there next launch. No key press is needed to save.
* IF A KEY DOES NOTHING: check the log for that press. The build prints a line
  for every press, including a CLAMPED line at the limits and a warning if the
  press is saved but cannot move the hand yet. A press with NO line at all is
  the interesting failure - that means the key is not reaching us.

**Test 3 - the weapon identifier.** Nothing to press. Draw the crossbow, stand
still somewhere quiet facing a wall, and keep it in view.

* EXPECTED, before you draw anything: `wid: WAITING - N component(s) resolved
  and none of them is a weapon`, listing what it has. This is the fix: last
  run it swept anyway and produced a confident wrong answer.
* EXPECTED, a few seconds after the crossbow is out: `wid: sweep planned -
  baseline plus N component(s) x 4 phases`, then the crossbow BLINKING off and
  on twice per component, about 1.5 s each. **Stand still for the whole
  sweep** - the test is "this draw stops and comes back with the hide", and a
  view that changes under it produces the same signal.
* EXPECTED at the end: a report. The three outcomes and what each means:
  * `'crossbow_01' OWNS signature ...` - this is the answer, and it is what
    the weapon attachment needs.
  * `*** THIS REPORT IS VOID ***` - the signature table filled up. Not a
    failure to report; it means try again somewhere emptier.
  * `'crossbow_01' owns NO signature` - hiding it stopped nothing that came
    back. Also an answer, and a different problem.
* The report also prints how many signatures vanished on ONE hide but not the
  other and were REJECTED. A nonzero number there is the instrument working:
  those are exactly what the previous build called owned.

### What changed this session

**The hand trim moved to BioShock Remastered VR's numpad scheme, per hand**
(`7c621cc2`). F5 could never have worked: it is the game's quicksave and
`head_track.cpp` reads it at two more places, so one press fired three
features. The scheme is adopted key for key because those are the keys the
tester already has in his fingers.

The trim is now per hand. One shared value assumed the residual after
calibration is common to both palms; it is not, since the two grips are solved
from two separate poses. The old shared keys seed both sides once so nothing
already dialled in is lost, and the migration is logged.

Every numpad key was already claimed behind a feature gate, so `[Hands]
Adjust=1` makes an EXPLICIT claim rather than adding another reader: the
census gives up Numpad 4-9 entirely, the material cycler gives up 2, and the
mesh split gives up Numpad 0 with its mode cycle moving to Numpad 1. One
startup line names what took what. Numpad + - * / . are untouched.

**The weapon identifier was rebuilt so it can fail its own hypothesis**
(`7bfa2675`). All three defects named in the previous entry are fixed:

1. it refuses to plan until a component that is not the player body has
   resolved AND the candidate list has been unchanged for three seconds;
2. a table overflow VOIDS the report instead of footnoting it, and the table
   is sixteen times larger;
3. each component gets HIDE, SHOW, HIDE, SHOW, and a signature is attributed
   only if it vanishes on both hides and returns on both shows. The report
   counts the single-cycle near-misses it rejected, which is the number that
   shows the old sweep was measuring noise.

A fourth thing turned up on the way: `MatShowSection` returns true when the
native was CALLED, not when the section actually hid - it logs `[DID NOT
TAKE]` separately and still returns true. Phases are now judged by reading
`HiddenMaterials` back, and a phase whose flags disagree with what it asked
for is poisoned and its component reported UNTESTED.

### The levers as installed

`Adjust=1 AdjStepT=1 AdjStepR=3 Palette=1 PaletteWorld=1 PaletteEyeOffset=1
PaletteDepthRange=1 PaletteRotate=1 WeaponId=1 WeaponIdMs=1500 MatCycle=0`,
per-hand trim under `TrimLTX..TrimRRZ`, grip calibration under
`GripLVersion`/`GripLParity`/`GripLX..Z` and the right-hand equivalents.
`MatCycle` must stay 0: the identifier drives the same hide/restore calls and
refuses outright while the cycler is armed. `PaletteRotate=0` returns to the
headset-confirmed translation-only build.

### Verified on the desk, not in the headset

27 frame-maths cases pass (`build\src\RelWithDebInfo\frame_test.exe`),
including `hand_trim_is_in_the_palm_frame` and `hand_trim_carries_the_weapon`
which pin the trim the numpad now drives. `tools\lint.ps1` clean, exports
clean. **Nothing here has been in a headset** - the numpad bindings, the
per-hand split, the restart path and the whole identifier are unverified.

### Next steps

1. The three tests above.
2. If the identifier names the crossbow's draws, weapon placement is unblocked:
   `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` is the full spec, and
   the weapon target comes from the SHARED `palm_target` helper the hands
   already use, so a weapon can be placed before either hand draws.
3. The hand adjust has no auto-repeat, deliberately - BRVR has none and an
   unrequested one overshoots. If the tester wants it, it is four lines.

## PREVIOUS CURRENT (2026-09-07): VR-33 - hands CONFIRMED, weapons are next

### Headset-confirmed this session

**The hands track position AND rotation correctly.** After one SHIFT+F7 grip
capture they snapped to almost exactly the right pose, tracked both position and
rotation, and stayed still when the head moved. The tester asked for this to be
kept. Recoverable tag: commit `82abe020`.

Before the capture they were mirrored and inside out. That is the same
arithmetic as the restart bug and both are fixed in `9d55ccde`: the solved grip
is a REFLECTION (det -1), three Euler angles cannot carry one, and identity was
therefore the wrong uncalibrated default. The record now stores a parity sign
beside a proper rotation, is versioned, refuses pre-version records, and saves
itself so a calibration survives a restart with no key press.

### The calibration save WORKS, measured

```
ms/palette/grip: SOLVED for the RIGHT hand ... parity -1, proper rotation
                 +29.52 -47.63 +22.47 degrees
ms/palette/grip: the left hand's calibration is SAVED (version 2, parity -1)
ms/palette/grip: the right hand's calibration is SAVED (version 2, parity -1)
```

Both hands solved parity -1, as predicted, and both wrote a version-2 record.
The restart path itself is still UNVERIFIED - nobody has relaunched and checked
the hands come back right without a key press. That is headset test 1 below.

### THE WEAPON IDENTIFIER PRODUCED A FALSE POSITIVE. Read this before reusing it

It ran, and its report is wrong. Three defects, all visible in its own output:

```
wid: sweep planned - baseline plus 1 component(s)
wid: 128 distinct skinned draw signature(s) over 16974 draw(s),
     55272 that did not fit the table
wid:   'Skm_Player' OWNS signature ... | c6 x3 (1 bones) prim 486 stride 12
```

1. **Only ONE component resolved.** `Skm_Player` alone; no `crossbow_01`, no
   `bolt_01`, no `Wpn_PlySword01`. The sweep fired about 17 s after the config
   loaded, before the weapons existed as components. It must WAIT until the
   assets it is there to identify are actually resolved, and refuse rather than
   sweep a list that cannot answer the question.
2. **The signature table saturated.** 128 held, **55,272 draws did not fit**.
   Once full it cannot record a new signature, so later phases are blind. An
   overflow must INVALIDATE the report, not appear as a footnote under
   attributions that it silently broke.
3. **The attributions are not weapons and are probably not the player.**
   `c6 x3` is a ONE-bone palette at stride 12 - world props, not the skinned
   first-person mesh. What the report actually measured is "these draws stopped
   during a 1.5 s window", which a camera move or an object leaving the view
   produces just as well as a hide does.

**The instrument cannot fail its own hypothesis, which is this project's oldest
recurring fault** (`VR-33-HANDS-AND-WEAPONS.md` section 4). The orphan count was meant to be
the control and a saturated table defeats it. The fix is not a bigger table
alone: a signature must vanish on EVERY hide and RETURN on EVERY restore, over
at least two hide/restore cycles, before it is called owned. A single
disappearance is not evidence.

### FEEDBACK FROM THE HEADSET RUN

1. **F5 is already bound.** The hand trim added in `9d55ccde` uses F5, and
   `head_track.cpp:620` and `head_track.cpp:1064` already read it. One press
   fires both, so the trim is unusable as shipped and must be rebound.
2. **Nothing was seen to blink** - consistent with the above: only the player
   body was ever hidden, for 1.5 s, once.
3. **The alignment needs small tweaks**, position and rotation, per hand.

### What the tester asked for next, specifically

**Adopt BioShock Remastered VR's numpad adjust scheme, per hand.** Four modes
cycled with **Numpad 9**, in this order:

```
  0  LEFT hand POSITION
  1  LEFT hand ROTATION
  2  RIGHT hand POSITION
  3  RIGHT hand ROTATION
```

The scheme is adopted from the maintainer's own BioShock Remastered VR mod,
where it has been in use for a long time; these are the keys the tester already
has in his fingers, which is the reason for matching them exactly:

| Key | Position mode | Rotation mode |
|---|---|---|
| Numpad 8 / 2 | forward / back (cm) | pitch (deg) |
| Numpad 6 / 4 | right / left (cm) | yaw (deg) |
| Numpad 0 / 5 | up / down (cm) | roll (deg) |
| Numpad 7 | cycles the step: 0.5 / 2 / 5 cm | 0.1 / 0.25 / 0.5 / 1 / 2 / 5 / 15 deg |
| Numpad 9 | cycles the mode, and the log line NAMES the mode and hand | |

Every change is logged and written back to the ini, as BRVR does.

**THE COLLISION, measured, and it must be handled before this ships.** Every
numpad key in this repo is already claimed, each behind a feature gate:

| Keys | Owner | Gate | Free right now? |
|---|---|---|---|
| 1 / 2 / 3 | the material cycler | `g_matCycleCfg` | yes - `MatCycle=0` in the installed ini |
| 4 / 5 / 6 / 7 / 8 / 9 | the draw census and its eighth-cutter | `g_dcOn` | only while the census is off |
| 0 and `/` | the mesh split | `g_msOn` | **NO - the split is what draws the hands** |
| CTRL+2 | hand move | `kCtrl` | n/a |

So Numpad 0 and 5 (up/down in BRVR's scheme) collide with the live mesh split.
**Do not just add another reader.** Give the adjust mode an explicit claim: when
`[Hands] Adjust=1` the adjust block takes the numpad and the split, census and
cycler blocks are suppressed for those keys, with one log line saying the numpad
is claimed and by what. One key firing two features has cost this project a
session already (`7099c3b0`, `0e1ccbb0`).

The existing hand trim (`g_mpTrimT` / `g_mpTrimR`, palm-frame, saved on every
press) is the right backing store for the two LEFT/RIGHT modes - but it is
currently ONE shared trim for both hands and needs splitting per side. It is
already applied through `palm_target`, so a per-hand version needs no new maths.

### Then: actually attach the weapons

The identification instrument is built and armed but has produced nothing yet.
Its output is the input to placement, and placement was deliberately NOT written
blind. `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` is the full spec;
the short form is:

* one stable grip/root frame on the crossbow;
* `D_assembly_C = WeaponGripTarget_C * inverse(S_C)`, and every member takes it
  through its own `LocalToWorld`: `D_j_local = inverse(L_j) * D_assembly_C * L_j`;
* the loaded bolt keeps its animated relationship to the root - independently
  pinning each part to a controller offset would cancel its animation;
* the weapon target comes from the SHARED `palm_target` helper, which is already
  extracted and tested, so a weapon can be placed before either hand draws;
* weapon draws must CONSUME the eye decision, never feed the hand classifier.

### The levers as installed

`Palette=1 PaletteWorld=1 PaletteEyeOffset=1 PaletteDepthRange=1
PaletteRotate=1 WeaponId=1 WeaponIdMs=1500 MatCycle=0`, grip calibration saved
under `GripLVersion`/`GripLParity`/`GripLX..Z` and the right-hand equivalents.
`PaletteRotate=0` returns to the translation-only build.

### Verified on the desk, not in the headset

27 frame-maths cases pass (`build\src\RelWithDebInforame_test.exe`, and the
same suite runs from `DllMain` into every log). The 28 saved packets replay
through the shipped decomposition: dominant slot 10 left / 35 right, uniform
scale 0.9995117 to 0.9995123, worst anisotropy 6.0e-07.

Measured this session and in ENGINE_NOTES: the XR-to-game pose mapping is a
MIRROR (`det(B*F) = -1`); the pose tick and the hand draws are ONE thread
(14224), 0 stale snapshots over 11,881 publications; and two of the three hand
shaders run normals and tangents through the same palette rows, so a rigid
correction carries the tangent frame and `WorldToLocal` must be left alone.

## PREVIOUS CURRENT (2026-09-07): VR-33 - rotation and grip built, then confirmed

### Run 1 found the fault, and the instrument named it

The first rotation build refused on **every** draw: `rotate=1 placed 0 refused
116908 (B*F is not a proper rotation)`. The hands looked unchanged and SHIFT+F7
appeared to do nothing, because the grip capture sits behind the same gate.

**The guard was wrong, not the game.** The draw's camera basis is RIGHT-handed,
so the pose mapping between XR and the game's camera-relative frame is a
MIRROR - exactly what a right-handed runtime and a left-handed engine produce.
A reflection is a coordinate convention: it carries through by full basis change
and CANCELS between the controller orientation and the grip transform, so what
reaches the palette is a proper rotation either way. The guard now requires
orthonormality only, records the parity, and a new self-test case
(`improper_basis_roundtrip`) pins the whole chain under a mirrored mapping:
capture exact, `det +1.0000`, a 37 degree controller turn giving a 37.00 degree
hand turn.

Two things the run confirmed on the way: the **fail-soft held** - all 116,908
draws still placed translation-only, so nothing regressed and the run simply
looked like the previous build - and the **pose tick and the hand draws are one
thread** (14224), with 0 stale snapshots over 11,881 publications.

A pending grip capture now logs a warning naming why it has not been consumed,
so a press can never silently do nothing again.

### What is armed right now

The installed build is Release with `[Hands] PaletteRotate=1` and the grip
transform at identity. **Launch the game; nothing else needs doing.** The full
test list, with expected outcomes and what each failure would mean, is section 3
of `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md`.

**The hands will start at a wrong ANGLE.** G is identity until it is captured,
so they track the wrists at a fixed offset. **SHIFT+F7** solves G for both hands
and prints six numbers for the ini; the hands snapping to the game's own
orientation at that instant is what the calibration means.

`PaletteRotate=0` returns to the headset-confirmed translation-only build.

### The one question this run answers

Does the candidate palm frame (`src` in `ms/palette/frame:`) move when the GAME
animates the hand, and stay put when only the fingers move? Nothing offline can
answer it: all 28 saved packets are one near-idle pose. If `src` never moves
during a melee swing or a power, the dominant palette slot is not the palm's
frame and the orientation source changes to a validated landmark fit.

### What was corrected before building

An external review found three blockers in the first draft of the plan, all
fixed:

1. **The motion gate was impossible.** It expected the game's palette to turn
   when the tester rolled a physical wrist, with no mechanism connecting them.
   Withdrawn; three orientations (`src`, `ctl`, `out`) are now logged under
   separate names and are expected to be INDEPENDENT while rotation is off.
2. **The orientation conversion reintroduced head-driven rotation.** A pose
   orientation maps controller-local axes into another frame; a similarity
   transform changes the basis of a rotation operator, which is a different
   object. The correct form is `O_C = B * F * transpose(R_head) * R_ctl` with
   `F = diag(1,1,-1)`, matching the physical mapping the working position path
   already performs. The counterexample is now a shipped test that the rejected
   formula fails by 180 degrees.
3. **The grip capture mixed spaces.** The source frame is component-local and
   the controller camera-relative; it is carried through the draw's own
   LocalToWorld first.

### Measured before any of it shipped

* **20 of 20 frame-maths cases pass** (`src/tools/frame_test`, and the same
  suite runs from `DllMain` into every log). They include the head-turn
  counterexample, the grip round trip, the pivot, and the proof that rotation
  OFF is bit-for-bit the shipped translation behaviour.
* **All 28 saved packets decompose** through the shipped code: dominant slot 10
  in every one, uniform scale 0.999511659 to 0.999512255, worst anisotropy
  6.0e-07, worst orthonormality residual 9.8e-07. Three orders of magnitude
  inside the tolerance.
* **The single slot's frame moves 1.05 degrees** across those packets where the
  weighted blend looked frozen to five decimals - so the slot does respond to
  the engine's animation. It is not yet evidence that it follows the PALM.
* **The three hand shaders' normal path is closed.** Two of them run normals and
  tangents through the same palette rows, so a rigid correction carries the
  tangent frame; their `WorldToLocal` is a view/light conversion and is NOT
  touched. ENGINE_NOTES carries it.

### Next steps

1. The headset run above. Report the six grip numbers and whether `src` moves
   with the game's animation.
2. Put the solved G in the ini and re-judge orientation.
3. Then the weapon assembly: a stable grip/root source frame, ONE root
   correction, and every member's animated transform preserved RELATIVE to that
   root. Independently pinning each animated part to a controller-relative
   target cancels its own animation.
4. Then firing. The visual weapon stage explicitly accepts that a released bolt
   returns to the native origin; correcting it is the firing stage.

Carried and deliberately not done: a render-view ticket for the eye, the
`same eye` counter (an instrumentation limit, not 776 repeated eyes - no
threshold tuning), head look-ahead against hand sampling time, one canonical
metres-to-units conversion (logging only for now), and mesh geometry size.

## PREVIOUS CURRENT (2026-09-07): VR-33 - the hands are at the controllers and correct

### Confirmed in the headset

The hands **track the controllers, are correctly scaled, occlude against world
geometry, and hold position through head turns.** Tagged `vr33-hands-working`.

**The full record is `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md`** - the mechanism, the
engine facts, the seven approaches that failed and why, and the instrument
failures that cost the most. Read that before touching this code; most of what
looks like an obvious improvement has already been tried and measured.

### The mechanism, in one paragraph

The bone palette's output is the component's LOCAL space; `LocalToWorld` (c231)
takes it to a camera-relative world frame and `ViewProjectionMatrix` (c0) to
clip. Placement re-skins the palm from the game's own palette every frame and
translates by `target - q`, so the animated baseline is subtracted rather than
left underneath. The camera basis comes from the VP's rows. The eye is decided
once per Present from the right-axis jump in `LocalToWorld`'s translation, whose
sign gives left or right absolutely. Register numbers are parsed from each
shader's CTAB, never hard-coded - three shaders draw this mesh and they
disagree.

### Next phase

1. **Rotation and grip.** The hands keep the engine's animated orientation, so
   they look posed rather than gripping. Full rigid composition, per-component
   conjugation `D_local = inverse(C) * D * C`, a stable palm frame from
   deliberately chosen landmarks rather than the current position patch, and
   basis conversion by conjugation - the translational basis has determinant
   -1 and is a coordinate convention, not a rotation.
2. **The weapon assembly.** Crossbow, loaded bolt and reload parts, moved by the
   same common transform through each component's own `C`. A hand-local delta
   cannot be copied into a weapon-local palette.
3. **Gameplay consumers.** Firing aim, projectile origin, melee. A GPU edit does
   not move them.

Carried and deliberately not done: one canonical metres-to-units conversion
(`[Hands] WorldScaleUU` 100 against `[PosTrack] Scale` 108), pose timing (head
look-ahead against hand prediction), a render-view ticket to carry the eye
rather than infer it, and a harness case for the feature-gate regression that
cost three runs.

## PREVIOUS CURRENT (2026-09-07): VR-33 - the hands are at the controllers; the stereo eye offset is the last placement fault

### Where this is

**Placement through the measured chain WORKS.** The hands track the
controllers, hold position through head turns, and now occlude correctly
against world geometry. The tester rates it the best result of the session.

The path is measured, not inferred. Read from the shaders' own disassembly:

```
p_local = ( sum_i w_i * BoneMatrix[idx_i] ) * ( v0 * MeshExtension + MeshOrigin )
p_cam   = LocalToWorld * p_local            (camera-relative world, uu)
clip    = ViewProjectionMatrix * p_cam
```

Placement is `target_local = Rl^T * (d_cam - t)`, `T = target_local - q_local`,
with `LocalToWorld` and `ViewProjectionMatrix` read from the device at each
draw through the register indices that shader's own constant table declares.
`ENGINE_NOTES.md` carries the full finding.

### The remaining fault: the eye offset is not applied

**Symptom:** correct in each eye individually, far too large with both open.

**Diagnosis:** `method=reentry` is live and the scene is genuinely drawn twice
(`L/s=88 R/s=88 mono/s=0`), so the world has correct stereo. But `d_cam` is
computed from the HEAD CENTRE and used unchanged for both eyes. Placing the
hand at the same camera-relative offset in each eye puts the two hand images an
IPD apart in world terms - the disparity of an object at infinity. A hand-sized
object at infinite disparity reads as a giant hand far away, which is exactly
what is reported.

**The fix** is to subtract that eye's offset: for each eye,
`d_cam_eye = d_cam_head -/+ (IPD/2) * right_axis`, with the right axis already
recovered from the ViewProjectionMatrix. IPD is known (63.0-63.1 mm measured).

**The blocker** is that the draw does not yet know which eye it is drawing.
The capture records the eye as `-2`, "not identified", which was flagged as a
known gap when the mono method made it harmless. Under `reentry` it is not
harmless and is now the last thing between this and correct hands.

### What is measured and settled

* Three shaders draw this mesh. Only one is depth-crushed (`MaxZ 0.001`); the
  other two use the full range. Placement currently applies to all three.
* Register layouts differ per shader and are parsed from each shader's CTAB.
  One shader defines c4 as an immediate that disagrees with the device.
* The shader does NOT normalise skin weights, but the anchor's weights sum to
  exactly 1.0000, so it is harmless here.
* The depth-range lever works: restoring `MaxZ` 1.0 for our draws gives correct
  occlusion against world geometry.
* Scale is still formally unmeasured, but the "huge" report is now attributed
  to the eye offset rather than to scale, and should be re-judged after it.

### Next steps

1. Identify the eye at the hand draw under `reentry`, and apply the eye offset.
2. Re-judge apparent scale once stereo is correct.
3. Then Build B: controller orientation and a grip-to-palm transform.
4. Outstanding and deliberately not done: pose timing (head look-ahead vs
   hand), the harness case for the feature-gate regression, render states in
   the capture, and a rigorous uu/m measurement.

## PREVIOUS CURRENT (2026-09-07): VR-33 - the SkelControl lane is closed, the palette route is next

### Where this branch is

The arm/hand split is healthy and confirmed in the headset: hands cut at the
wrist, arms hidden, caps present, cap colour approved, ring at the measured
-4.9. Two PRs are open and unmerged - #19 (VR-31, the split) and #20 (VR-53 and
VR-51, the desktop mirror eye pin and the pause-menu session loss).

**VR-33's native route is closed, and it is now evidence rather than an
artifact.** The earlier "zero of 64 SkelControl objects advance" reading was a
truncated scan - the sweep stopped when its 64-entry comparison table filled,
so 64 was the array's capacity, not a population. Fixed and re-run the same
day with the hands subsystem LIVE (`[Hands] Enabled=1`, `[Mode] GamepadOnly=0`,
the state every earlier measurement lacked), 34 consecutive samples read:
**103,117 GObjects entries walked, 66 SkelControls, all 66 tracked, 0
untracked, 0 advancing.** No SkelControl is evaluated on this build, so a write
to one cannot move anything. The old table had missed exactly two objects, so
the retracted reading was right by luck; it is measured now. This also does
retire the 38.x "9,000 writes a second outrun the recompute" reading. See
ENGINE_NOTES, "SkelControls are NOT evaluated on this build".

The scan's cost is measured too: **505-520 ms of game-thread stall once per
second**, attributed by the perf line to `out/idle`, ~46 display slots at
90 Hz. It is unplayable while armed. It is back OFF in the installed ini.

### What IS established, and is worth keeping

* `handAttachment_L/R_jnt` are children of `hand_L/R_jnt`, from validated
  engine parent walks. If anything ever does move a hand joint, the weapon
  attachment is beneath it.
* The camera is on the spine/head branch; the two arms meet only at `Root_jnt`.
  A per-side edit at or below a hand cannot disturb the view or the other hand.
* Skeleton indices are NOT palette slots: `hand_L_jnt` is 54 and
  `handAttachment_L_jnt` 56, against a 48-entry palette.
* The arm mesh's declaration uses streams 0 and 1; a stale stream-2 binding was
  what kept costing the wrist caps. Bound is not used.
* Full bone table, class Super offset (+0x44), socket table with parent bones,
  and the control field offsets - all in ENGINE_NOTES.

### The next step

**The draw-scoped palette backend, hands only.** Build a private palette per
hand by applying one common rigid delta D to every skinning matrix that draw
consumes, and draw each hand under its own palette. Finger animation survives
because `sum_i w_i (D M_i) v = D (sum_i w_i M_i v)`, so this is not a static
hand. The two hand classes already have independent index ranges in `MsDraw`,
which is where it goes.

Its honest cost: it moves pixels only. Weapons, muzzle effects and firing aim
stay on the engine's transform, which a GPU edit does not touch, so the weapon
half of VR-33 needs a separate mechanism. The tester has already accepted that
the crosshair can be faked separately.

The gate is passed - the tick scan has been run and the native lane is shut.
The palette backend is the route, and it is the work in front of this branch.

**What it is not**: it moves pixels only. Weapons, muzzle effects and firing
aim stay on the engine's transform, which a GPU-side edit does not touch, so
the weapon half of VR-33 needs a separate mechanism. The crosshair can be
faked separately and that has been accepted.

### How the pieces already on disk fit

The backend is a join of two things that exist, not new machinery.

* `hkSetVSConstF` already carries a whole-palette rewrite - the 30.70/71 hand
  drive in `core/framework/vs_const_hook.cpp` applies a per-hand rotation and
  translation to every bone matrix in a `c6` upload. That is exactly the
  `D * M_i` the finger-animation argument needs.
* What it lacks is DRAW SCOPE: it identifies a rig by upload `count` and
  ordinal, so it cannot give the two hands different deltas when they share
  one upload.
* The missing half is on the other side. `MsDraw` in
  `game/dishonored/hands/mesh_split.cpp` already has independent per-class
  index ranges (`MS_CLS_HAND_A` / `MS_CLS_HAND_B`).

So: cache the game's last `c6` block, and have `MsDraw` upload `D_L * M`
before the hand-A range and `D_R * M` before hand-B, restoring the original
block afterwards. D3D9 constants are current state, not one-shot - the trap is
already recorded at `hands/draw_census.cpp:334`.

### Build and deploy state

The installed `d3d9.dll` is code-current with this branch - built and installed
2026-09-07 12:55, three DOCS-ONLY commits behind HEAD. Its last run stamped
`alpha-334-g88eefb61-dirty`; HEAD is `alpha-337-g220c5a28`, and the difference
is ENGINE_NOTES and STATUS only. Rebuild anyway before trusting a build id.

**Verified in the headset:** the split, the clip, the caps and cap colour, the
ring at -4.9, the arms staying hidden, and the SkelControl lane being inert.
**Built but NOT headset-verified:** the desktop mirror eye pin and the
pause-menu session fix on PR #20 - both still need the run in
`docs/dishonored/DESKTOP_MIRROR.md` section 7.

**All diagnostics are now disarmed on the dev rig**: `[Hands] BoneQuery=0`,
`HandMoveTest=0`. `PoseReport` has no key and defaults on; it is read-only and
prints once. The GObjects tick scan is gated behind `HandMoveTest` and is the
thing that cost frame rate - leave it off.

### Traps this session paid for

* A too-narrow grep produced two confident false claims ("the codebase has
  never called a UE3 function", "g_peReentry is read nowhere"). Both were
  wrong; grep the tree, not one file.
* An instrument that cannot fail its own hypothesis is worse than none: a
  function-local `static` made a false negative read as a measurement, and a
  walk that printed ROOT for three different endings made a broken chain look
  complete.
* The GObjects-wide tick scan is heavy enough to be felt in the headset. It
  ships OFF and should stay off.

## PREVIOUS (2026-09-06): VR-33 - hands and weapons where the controllers are

This is the working branch for VR-33 and it is the CONTINUATION OF BOTH open
PRs: the arm/hand split (VR-31, PR #19) and the desktop mirror eye pin plus the
pause-menu session fix (VR-53 / VR-54, PR #20). Both are merged in here, and
neither has been merged to `VR-Main`.

### Read this before installing anything

**The two PR branches do not work on their own.** Branch VR-31 has no API layer
guard, and without it `xrCreateInstance` fails with `XrResult(-32)` on both the
native runtime and the SteamVR shim, so the game runs flat with no VR at all.
The guard is on the VR-53 branch. That was found by installing the VR-31 branch
alone on 2026-09-06 and losing VR entirely.

**So install from THIS branch**, not from either PR branch, for as long as both
PRs are open. The PR branches are for review; this one is what runs.

### What VR-33 is

The hands and the weapons go where the VR controllers are. VR-30 took the head's
yaw out of the pawn's facing, which is what lets a hand sit still in the world
while the head moves; VR-31 decided the presentation and cut the hands free of
the arms. This branch is the transform work that follows from both.

Nothing has been written for it yet.

### Open questions carried in from the two PRs

* **The cap's colour has not been re-confirmed** since the UV decode was
  widened. The mode had never run - this asset packs TEXCOORD0 as `FLOAT16_2`
  and the check demanded a `FLOAT2` - so every cap took ring vertex 0, an
  arbitrary choice that happened to look right.
* **Whether the ring's shape changes with the POSE** is unanswered. The cut is
  computed in the bind pose and seen in the animated one.
* **The desktop eye pin and the pause-menu fix are unverified in the headset.**
  Section 7 of `docs/dishonored/DESKTOP_MIRROR.md` has the three checks.

### References

* `docs/dishonored/ARM_HAND_SPLIT.md` - the split, every key and hotkey, the traps
* `docs/dishonored/DESKTOP_MIRROR.md` - the eye policy and the hold fix
* `docs/dishonored/BRIEF-eye-flicker.md` - the hypothesis graveyard, ANSWERED

## MERGED IN (VR-53 / VR-54, PR #20) (2026-09-06): VR-53 / VR-54 - the desktop had no eye policy, and a hold banked empty layers

This branch is the frame path and nothing else. The arm/hand split worked on in
the same sessions was split out onto its own branch and is VR-31.

**The full reference is `docs/dishonored/DESKTOP_MIRROR.md`**; the hypothesis
graveyard that led to it is `docs/dishonored/BRIEF-eye-flicker.md`, now marked
answered. This section is the handoff summary only.

### What was actually wrong

`hkPresent` calls the game's original `Present` for EVERY eye draw, and
`mirror_present()` in the runtime layer had never implemented the D3D9 copy -
its own comment said so. So the game WINDOW showed L(k), R(k), L(k+1), R(k+1)
while the headset received correct pairs the whole time. A recording of the
window alternates between two camera positions one IPD apart, which is exactly
what alternate-eye rendering looks like, and the diagnosis had been aimed at
the headset path for several sessions on the strength of it.

The headset was never doing AER. The desktop had no eye policy at all.

`core/gfx/desktop_eye.cpp` pins it: snapshot on the left eye's present, re-blit
over the right eye's present AFTER that eye's XR capture. The runtime layer
owns the WHEN and the new module owns the HOW, so `openxr_runtime.cpp` gains a
hook pointer and nothing else.

### The pause-menu session loss (VR-54)

On a hold-only present the submitted copies are `holdProj` / `holdViews` /
`holdQuad`, not the empty `proj` / `projViews` / `quad` locals - but the hold
sets `layerCount = 1` and the snapshot bank keyed off `layerCount`, so it
overwrote a good snapshot with zeroed structures and left it marked valid. The
next hold submitted null handles and a zero view count, `xrEndFrame` answered
`XR_ERROR_HANDLE_INVALID`, and the session stood down. Banked on
`builtNewLayer` now.

Still open and tracked separately: a saved layer holds swapchain HANDLES, not
pixels, and OpenXR composites the most recently RELEASED image, so preserving a
completed PAIR needs retained images rather than a retained structure.

### Retracted, not tuned

The "30 % of ticks double" reading and the stand-down guard built on it are
REMOVED. That window straddled a pause menu, an `xrEndFrame` failure and
session teardown; the windows either side read 78/78, 86/86, 87/87, 81/81. The
guard would have disarmed a healthy renderer every time a session dropped. The
`camera/eyetrace` line is corrected too - its ring samples constant uploads,
not presents.

### The run this needs

Look at the game window: one view, no alternation, while the headset keeps
correct stereo depth. `desktopeye:` in the log every 15 s should show snapshot
and re-blit counts EQUAL and non-zero. Then open and close the pause menu
several times: no `XR_ERROR_HANDLE_INVALID`, no session teardown.

### Not addressed here

Performance: ~78 complete pairs/s against a 90 Hz headset, ~9.7 ms of D3D9 GPU
span per tick, 15.7 Mpixel per pair at 2750x2850. That is the next subject.
## MERGED IN (VR-31, PR #19) (2026-09-06): VR-31 - the hands are cut from the arms, clipped and capped

This branch is the arm/hand split and nothing else. The desktop mirror eye pin
and the pause-menu session loss found in the same sessions were split out onto
their own branch and are VR-53 and VR-54.

**The full reference for everything below is `docs/dishonored/ARM_HAND_SPLIT.md`**
- the derivation, every ini key and hotkey, how to read the log, the traps, and
what is still unverified. This section is the handoff summary only.

### Where it stands

The arms and hands are one skinned triangle list with one material, so the
geometry is cut by us. The cut is derived per arm from BONE INFLUENCE, shaped
as a plane perpendicular to the forearm, with the triangles that straddle that
plane CLIPPED at it so the boundary is the plane itself rather than a row of
triangle edges. The open end that leaves is CAPPED with a two-sided disc
coloured from the mode of the boundary ring's own texture coordinates.

The ring ships at the tester's measured position: `[Hands] WristCutA` /
`WristCutB` = **-4.9**, read off the `ms/wrist` line at the end of the
2026-09-06 headset walk. It is asset-relative, so it survives a level load.

### Verified in the headset

* The split itself: the hands draw, the arms do not.
* The plane, the clip, and the ring walk with Numpad + / -.
* The cap, at the point where it was still falling back to ring vertex 0 for
  its colour.

### NOT verified

* The cap since the UV decode was widened. The log had been answering `NO
  TEXCOORD0` because this asset packs its texture coordinate as `FLOAT16_2` and
  the check demanded a `FLOAT2`, so the colour MODE never ran and every cap took
  ring vertex 0 - an arbitrary choice that happened to look right. The mode now
  runs, so **the cap's colour may differ from the one that was approved**. That
  is the first thing to look at on the next run.
* Whether the ring's shape changes with the POSE. The cut is computed in the
  bind pose and seen in the animated one. If the ring's shape changes as the arm
  moves, a slant has a different cause and a different fix than the axis; if the
  slant is fixed relative to the arm whatever the pose, it is the axis. One run
  settles it and nothing else can.

### The run this needs

Look at your hands. The wrist ends in a solid disc, no see-through, from every
angle including down the arm from the elbow end. Walk the ring with Numpad
+ / - and watch the colour track it - sleeve on the cuff, skin past it. Then
say whether the colour is the same one as before, because the decode fix could
have changed it.

In the log: `ms: cut end CAPPED` names the colour path that ran and the winning
ring vertex; `ms: triangles by class` carries the whole result in one line.

## PREVIOUS (2026-09-06, session 20d): VR-31 - a reversed winding was eating a third of the cut

The clip RAN - the log says 54 triangles cut into 108 new vertices, stream 0
ours - and the result was still ragged, with some of the boundary on one clean
line and the rest missing or floating loose. That pattern named the bug.

### The bug: one third of every clip was wound backwards

`MsClipTri` picked the three corners by ASCENDING INDEX (`in[0], out[0],
out[1]`) instead of in cyclic order. When the odd vertex out is the MIDDLE one
of the three - one case in three - that reverses the triangle, and a reversed
triangle is culled. Two thirds of the cut came out on a clean line and one
third vanished, which is exactly what was reported.

Fixed by taking the odd vertex `i` and its neighbours as `(i+1)%3` and
`(i+2)%3`, so both halves keep the source triangle's winding.

### The slant: the ring may be square to the wrong direction

"Taking too many on the other side of the forearm" is a ring that is not
perpendicular to the arm. The axis was the longest direction of the arm's
triangle cloud (PCA), which a tapered sleeve can lean off the bone.

`MsBoneAxis` derives it from the SKELETON instead: the hand bone minus the bone
it hangs off, that bone being the hand's graph neighbour whose centroid is
farthest away. No names, no hierarchy, same as everything else here. Both axes
are computed, the angle between them is logged per side, and `ms axis
bone|pca` switches live - which one is right is a question about this asset, not
about geometry, and one press settles it. Default is the bone.

### Also

`+` / `-` auto-repeat after 400 ms, because at the ultrafine step the ring moves
0.1% of the arm per press and placing it by hand would be a hundred taps.

### Still unruled-out, and the check for it

The cut is computed in the mesh's BIND pose and seen in the ANIMATED one. If
the ring's shape CHANGES as the arm moves, that is the cause and the fix is a
different one; if the slant is fixed relative to the arm whatever the pose, it
is the axis. That is a thing the tester can see in one run and nothing else can
answer.

## PREVIOUS (2026-09-06, session 20c): VR-31 - the straddling triangles are CLIPPED

Headset run on the plane cut: the steps were much better, the edge was still
jagged - spikes hanging off the cuff in the screenshot. Diagnosed and fixed
here, untested.

### Why a plane was not enough

The plane fixed the JUMPS but not the OUTLINE. A triangle straddling the cut
was still kept or dropped whole, so the boundary was a sawtooth one triangle
high; on the coarse cuff geometry that is a set of spikes. No edge RULE fixes
that - keeping only triangles entirely past the plane trades spikes for
notches. The boundary has to stop being made of original triangle edges.

### The clip

A triangle crossing the plane is split into the part on each side, with new
vertices interpolated along the two crossing edges. Both halves are emitted
into their own class, so `arms` stays the exact inverse of `hands` and `all`
still rebuilds the original mesh. The boundary is then the plane itself.

That needs a VERTEX buffer of ours as well as an index buffer, because the new
vertices do not exist in the game's. Every stream-0 element is interpolated by
its declared type; blend INDICES come whole from the nearer parent vertex,
because a bone index is a name and not a quantity, and the weights go with them
so they cannot end up naming the wrong bones. Both buffers are `D3DPOOL_MANAGED`
with room to grow, so moving the ring does not recreate them and `hkReset` has
nothing to release.

**The one precondition, checked and logged**: stream 0 must be the only stream,
because re-basing the index list onto a buffer of ours would desynchronise any
second stream the game had bound. A mesh with more streams falls back to whole
triangles with the reason named.

### Smaller steps

`Numpad .` cycles the knob's step: 2% of the arm, 0.5%, 0.1%. Default 0.5%
(`[Hands] WristStep`).

### New defaults

`WristEdge=3` (clip), `WristStep=1` (fine). `ms edge 0|1|2|3` still selects the
whole-triangle rules for comparison - 3 is the only one whose boundary is the
plane rather than a row of triangle edges.

### The run this needs

The wrist should end in a clean ring with no spikes. `ms status` says how many
triangles were cut and how many vertices were made; if `stream 0 belongs to the
game` appears, the clip was vetoed and the reason is in the read lines. Then
`Numpad .` for finer steps, `+` / `-` to place it, and the `ms/wrist` numbers
become `WristCutA` / `WristCutB`.

Compare against `ms edge 1` to see what the clip is worth - that is the best of
the whole-triangle rules and should still show a notched edge.

## PREVIOUS (2026-09-06, session 20b): VR-31 - the cut is a PLANE across the forearm

The bone-influence split works and was confirmed in the headset: both hands
present, both arms gone, at `WristScale` 0.70 on both sides (radius 21.7,
2109 hand triangles per arm). Two faults in the SHAPE of the cut, both fixed
here and neither yet tested.

### The sphere moved in whole bones

The tester's walk is in the log and it is unambiguous: from scale 0.50 to 0.67
the triangle counts did not change AT ALL, then one press flipped 156 triangles
per arm at once. A bone is inside the sphere or outside it, so every triangle
that bone dominates changes class together. What is left is the outline of a
bone's influence region, which is why moving the knob made the edge jagged
rather than moving it.

### A plane cuts a cylinder in a circle

`MsPlaneDerive` per arm:

1. **The limb axis** by power iteration on the covariance of the arm's triangle
   centroids. Two passes - a rough axis over the whole arm, then a refined one
   over a band around the rough cut, so the ring is perpendicular to the
   FOREARM and not to the whole limb with the hand's mass pulling on it. The
   refinement logs how many degrees it moved, and refuses past 40.
2. **The starting position** keeps exactly the number of triangles the sphere
   was keeping at the tester's 0.70. Changing the shape of the cut does not
   move it: the look that was settled on survives, it just stops being blobby.
3. **The knob moves the plane in LENGTH** - 2% of the arm per press - so the
   ring travels smoothly instead of waiting for a bone to flip.

The sphere is kept, not deleted: it is the seed, the fallback
(`ms shape sphere`), and the thing that proved the classification works.

### New keys and defaults

`[Hands] WristPlane=1` (plane), `WristScaleA/B=0.70` (the tester's measured
seed), `WristEdge=0`, `WristCutA/B` unset (derive). Setting `WristCutA/B` pins
the ring in mesh units from the hand bone and survives a level load - the knob
and `ms cut` both print exactly that number, so a good look becomes the default
without another walk.

`ms cut <a> [b]` places the ring, `ms shape plane|sphere`, `ms edge 0|1|2`
(kept when the centroid / all three vertices / any vertex is past the plane).

### The run this needs

Look at the hands: the starting picture should be the SAME amount of forearm as
the tester left it, but ending in a clean ring instead of a jagged edge. Then
`Numpad +` / `-`: the ring should slide smoothly, a small step each press, with
no chunk of triangles vanishing at once. When it looks right, read the
`ms/wrist` line for the two numbers and they become `WristCutA` / `WristCutB`.

If the ring comes out slanted rather than square across the forearm, the
`forearm axis refined N degree(s)` line says how far the second pass moved and
is where to look. `ms edge 1` is the alternative if the one-triangle sawtooth
is visible up close.

## PREVIOUS (2026-09-06, session 20): VR-31 - the cut is DERIVED from bone influence, per arm

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open). Installed
Release. **Untested - built and installed, not run.**

### What replaced the eyeballed mask

The 32-slice mask cut by PERCENTAGE of the triangle order, and the two arms sit
in different, non-aligned regions of that order, so a mask tuned on the left
hand took too much of the right. No amount of further eyeballing fixes a cut
that cannot express the shape.

`src/game/dishonored/hands/mesh_split.cpp` classifies each TRIANGLE by the
bones that actually move it, which is per-arm by construction:

1. **Read once.** The draw's index range and vertex window are copied out of
   the game's own buffers - the first time this project has read them - and
   VALIDATED against facts the data must satisfy (indices inside the draw's own
   vertex window, bone indices below the palette size, weights summing to 1,
   finite positions, a non-degenerate bbox). A `D3DUSAGE_WRITEONLY` buffer may
   legally hand back an uninitialised page on a read lock; that is why the
   descriptors are logged BEFORE the lock and why a failed validation refuses
   instead of carving the mesh up from noise.
2. **Two arms from the SKINNING GRAPH.** Two bones are adjacent when some vertex
   is meaningfully moved by both. Separate limbs share no vertex, so the graph
   falls into two components - no bone names, no hierarchy, no engine
   structures. A geometric widest-gap split is the fallback and says so in the
   log when it is used.
3. **The wrist from the bone SPACING.** The hand bone is the one with the most
   neighbours (a forearm joins two things; a hand joins the forearm and every
   finger). Sort the side's bones by distance from it and the biggest gap in
   that list is the wrist. Derived, not eyeballed.
4. **Per triangle**, the class holding most of its influence weight wins, so a
   triangle straddling the wrist follows the bones that move it. Stray sleeve
   shards go with the arm bones they are weighted to, which is why they should
   disappear without a special case.
5. **One index buffer of our own** (`D3DPOOL_MANAGED`, so `hkReset` has nothing
   to release), classes emitted contiguously with both hands adjacent - so
   "both hands, no arms" is ONE draw call, not a run per fragment.

### Also fixed: the stale-lock weakness

The mesh lock is now **auto-armed by the mesh's measured signature** (prims
4448, verts 2771, skinned declaration, triangle list - all ini keys), and
`DcTick` releases an automatic lock that has not been drawn for 300 frames so a
level load that recreated the buffers re-arms on the new pair. An automatic lock
**fails soft**: a mesh it cannot split is drawn exactly as the game asked, on
both the indexed and non-indexed paths. A hand-armed lock (Numpad 6) is
unchanged and still drops what it is told to.

### Defaults and controls (everything ships ON)

`[Hands] ArmSplit=1 ArmSplitAuto=1 ArmSplitMode=1` (mode 1 = HANDS).

    Numpad 0   next mode: hands -> all -> arms -> other -> off -> hands
    Numpad +   keep MORE as hand (the cut moves up the arm)
    Numpad -   keep LESS as hand (the cut moves toward the fingers)
    Numpad *   which arm + / - moves: both -> side A -> side B
    Numpad /   re-derive the whole split from the buffers
    Numpad 5   release everything, and stop the automatic re-arm

Seam: `ms status | off | hands | arms | all | other | rebuild | wrist <n> |
side <n>`. The 32-slice mask and `dc mask s19` are untouched and remain the
fallback if a driver refuses the read.

### The run this needs

Load a save with the sword and crossbow out and read `dishonored_vr.log` for
`ms:`. Expected: `ms: ib fmt=... usage=...`, then a validation line with zeros,
then two graph components, two wrist radii, and a class count with BOTH hand
classes non-zero. In the headset both hands should be present and both arms
gone, with no percentage tuning at all. `Numpad 0` once shows the whole mesh
through our buffer (the A/B - it must look exactly like stock); a second press
shows the inverse cut, arms with no hands, which is the cheapest proof the
classification is real and not a coincidence.

If a hand is too short or too long, `Numpad +` / `-` moves that wrist and the
log prints the radius - ONE number per arm instead of 32 slice bits.

### Still not done

- The acceptance list: Blink, Devouring Swarm, Windblast, weapons, head and
  stick turns, crouch, reload, checkpoint reload. Power effects are
  socket-driven, so verify the SOCKETS follow, not just the fingers.
- `HmDrawIntoEye` asks the METHOD whether it wants a projection layer, not
  whether the RUNTIME submitted one.
- `FindRefSkel` does not enforce the validation its comment claims.
- Controller wrist placement is still a separate gate; `[Hands] Enabled=0`.

## PREVIOUS (2026-09-06, session 19): VR-31 - FLOATING HANDS WORK, the cut needs per-arm ranges

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open). Installed
`alpha-316`. **No PR opened - the user asked for commit and push only.**

### The result: the game's OWN hands, floating, with their own animation

Headset-confirmed twice with screenshots: Corvo's real hand holding the sword,
and the real hand holding the crossbow, with the arm gone from the cuff. This is
Arkane's geometry, Arkane's skinning and Arkane's animation - nothing is
replaced - so the powers animate the fingers exactly as they always did, which
was the whole requirement.

### How it works, and why it needs no index buffer

The first-person arms and hands are ONE skinned mesh (`bones=48 skin=IW`, 4448
triangles, 2771 vertices) drawn by three vertex shaders. The mesh is a triangle
LIST, so any contiguous run of its triangles can be drawn on its own by shifting
`startIndex` and shrinking `primCount`. **Nothing is read, captured or
replaced** - the game's own buffers stay bound and we ask for part of the list.

The target is identified by BUFFER PAIR (stream-0 VB + IB), not by bone-palette
size, so every pass over it is caught including ones the palette-gated census
never saw. `DrawPrimitive` is hooked as well as `DrawIndexedPrimitive`.

### THE MEASURED CUT, and its limitation

The tester's mask, 12 of 32 slices: **`0x3001D237`**

    slices  1,2,3   triangles  0%.. 9%
    slices  5,6     triangles 12%..18%
    slice  10       triangles 28%..31%
    slice  13       triangles 37%..40%
    slices 15,16,17 triangles 43%..53%
    slices 29,30    triangles 87%..93%

Restore it with **`dc mask s19`** instead of repeating twelve headset presses.

**It is NOT the default and must not become one yet.** It was tuned for the
LEFT hand and takes too much of the right. The reason is structural and is the
next problem to solve: **the two arms occupy different, non-aligned regions of
the one triangle list**, so a single set of slices tuned by eye on one hand
cannot be correct for the other. The slices are also PERCENTAGES of this mesh's
primCount, so the mask means nothing on a different asset or LOD.

### Next steps

1. **Two independent masks, one per hand.** The cut has to be chosen per arm,
   because the arms are not symmetric in the triangle order. Either two masks
   the tester tunes separately, or - better - classify triangles by BONE
   INFLUENCE so the cut is derived rather than eyeballed.
2. **Finer than 32 slices** where a slice straddles hand and arm. 32 was enough
   for the left hand and not obviously enough for the right.
3. **Sleeve shards.** Small black triangles survive the cut (visible in the
   session screenshots); they are weighted to arm bones and sit outside the
   marked runs.
4. **Then the acceptance list** from review: Blink, Devouring Swarm, Windblast,
   weapons, head and stick turns, crouch, reload, checkpoint reload. Power
   effects are socket-driven, so verify the sockets follow, not just the
   fingers.
5. **Controller wrist placement is still a separate gate** and untouched;
   `[Hands] Enabled=0`.

### Known weaknesses in the instrument, deliberately not fixed

- The mesh lock is armed from a censused row, so a level load that recreates the
  buffers leaves it stale and silently drawing everything. Needs a re-arm path
  before this is a shipping lever rather than a diagnostic.
- `HmDrawIntoEye` asks the METHOD whether it wants a projection layer, not
  whether the RUNTIME submitted one.
- `FindRefSkel` does not enforce the validation its comment claims.

## PREVIOUS (2026-09-06, session 19): VR-31 - the cycler is WALKED, the hands had an uninitialised matrix

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`). Installed build `alpha-308`.

### VR-31's question is ANSWERED: route (d) cannot give hands

The cycler was walked in the headset on `alpha-307`. Four presses of Numpad 3
removed, in order: **both arms and hands together**, sword, crossbow, bolt -
exactly the plan the engine's own `GetNumElements` produced. Attribution is now
MEASURED, not recalled.

So `Skm_Player` carries arms and hands as ONE material section, route (d) gives
**floating weapons**, and it can never give floating hands.

**Note the limit of that conclusion** (raised in review, and it is right): one
shared material section rules out a MATERIAL-only split. It does not prove our
own hands are the only possible route - preserving Dishonored's original
animated hands would need a different geometry-FILTERING approach (route (b),
the c6 bone palette, x144 = arms, is untouched and is where that would start).
Custom hands are the route being taken, not the only one that exists.

### The hands drew every triangle and showed nothing - the cause is found

First run of the rebuilt hand pass: `calls=480 draws=480 tris=120960 last=drew`,
252 triangles per present (both models, complete), nothing on screen.

**Cause, found by code review rather than another run:** the transform did
`float B[9]; RtdBuildYPR(rad, B);` and `RtdBuildYPR` is compiled ONLY under
`-DDVR_WITH_LEGACY=ON`. Every shipped build takes the empty stub in
`src/legacy/legacy_stubs.inc`, so `B` was never written and every hand vertex
was rotated by nine floats of stack garbage. The counters stayed healthy because
the geometry really was built and submitted; and a NaN passes the near-plane
reject, because `NaN > -0.03f` is false.

Fixed in this build:

- **`HmBuildYPR`, local to `hand_mesh.cpp`** - the twelve lines needed, not the
  legacy subsystem. Y-up controller convention (yaw about Y, pitch about X, roll
  about Z), so the ini keys mean what they say. Verified numerically: exact
  identity at zero trim, equal to `Ry*Rx*Rz` to 2.2e-16 over 64 combinations.
  Inert in the current configuration - every shipped trim value is 0.0.
- **`submitted` and `ON-SCREEN` are different numbers now.** The beat also counts
  `nonFinite`, `nearPlane` and `degenerate` (zero projected area draws no pixel
  and has a perfectly ordinary bounding box) and prints the NDC bounding box, so
  "invisible" resolves to an axis and a magnitude.
- **Real depth.** `pos.z = 0.5f * (-z)` over `w = -z` divides to exactly 0.5 at
  every distance, so the depth buffer could order nothing. Standard mapping now.
- **The calibration triangle** (`[VRHands] CalibTriangle`, `vrhands calib on`),
  default OFF: one fixed-NDC triangle through the same shader, buffer, states
  and target. The FALLBACK if the hands are still invisible once the geometry is
  sound - and its outcomes are not fully exclusive, so the beat's counters, not
  it, are the first read.

**How far the bug class extends, measured:** of 23 stubs in `legacy_stubs.inc`,
exactly one had an out-parameter a live caller read unconditionally, and it is
this one. `RtdSnapshot` also has out-parameters but returns false and its caller
gates every read on that. The other 21 are void no-ops, as intended.

### Still open, deliberately not fixed in this build

- **The projection guard reads the wrong thing.** `HmDrawIntoEye` asks the
  METHOD whether it wants a projection layer, not whether the RUNTIME submitted
  one. It did not misfire on this run. Fixing it means adding an accessor to
  `openxr_runtime.cpp`, the file this project keeps closest to the BioShock
  copy, so it is not being done mid-diagnosis.
- Hiding the game's arms once ours are visible should use the **proven
  material-section hide on `Skm_Player`**, not `[VRHands] HideGameArms` - that
  flag's upload-size filters also target weapons and carry static-geometry
  heuristics, so it is not equivalent.

### The requirement changed: the REAL hands, because the powers animate them

Custom meshes cannot carry Arkane's power animations, so the working custom
renderer is now the FALLBACK and route (b) is the goal. Full plan and the nine
review corrections are in ENGINE_NOTES, "route (b) starts at the DRAW".

**Step 1 is built and installed: the DRAW census** (`hands/draw_census.cpp`,
`[Hands] DrawCensus`, ships ON). Upload SIZE cannot settle an arm/hand split -
two chunks can share a size, several draws can reuse one upload, and `HideSizes`
only contains what someone already saw. So `DrawIndexedPrimitive` is hooked
(vtable 82) and every palette-fed draw is identified by what the renderer held:
stream-0 VB and stride, IB, vertex declaration, vertex shader, index range,
primitive count. **Numpad 6** next, **4** back, **5** all visible - one row
hidden per press, the log names it. `dc report|status|hide <n>|show` on the seam.

Two rules it observes: no engine D3D reference survives the detour (each `Get*`
AddRefs and is released on the next line, pointer kept as an identity token
only), and the hotkey posts a request that the tick acts on - the detour itself
runs on the RENDER thread.

### Next steps

1. Walk the draw cycler (Numpad 6). Two rows sharing a bone count but differing
   in VB/IB or index range are separate geometry a size-based hide cannot tell
   apart - that is the question. If one row is arms-without-hands, route (b) is
   a draw skip and it is nearly done.
2. If arms and hands share one draw: collapse-to-wrist as a cheap prototype -
   with the wrist point derived in the palette's OUTPUT space, NOT read off a
   translation column (skinning matrices fold in the inverse bind pose), and a
   ZERO 3x3 linear part, not identity.
3. If that seams: the index-buffer filter - a replacement index list of hand and
   cuff triangles only, original vertices, weights and animated palette kept.
   Survives arms and hands sharing both material and chunk.
4. Controller wrist placement is a SEPARATE gate; `[Hands] Enabled=0` today.
5. Read the hands beat line. `ON-SCREEN=0` with `submitted>0` is a geometry fault
   and the NDC box names the axis; `nonFinite>0` means the transform is still
   producing NaN; healthy counters with a blank screen means arm the calibration
   triangle.
2. Then hide `Skm_Player` by material section for the floating-hands verdict.
3. Floating weapons as a shipping lever (hide `Skm_Player`, default OFF, live
   A/B); check the shadow and Blink's aim.

## PREVIOUS (2026-09-06, session 19): VR-31 - our own hands are WIRED (unverified)

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`, not `Fixes VR-31`). Installed build carries the hand wiring.

### The cycler is installed and has NEVER been run

`alpha-305-g23ef0ae5` (the cycler commit) is the installed proxy - verified by
reading the build tag out of the installed `d3d9.dll`. The newest log on disk is
`alpha-304-ge1135d25`, the run that proved route (d) with the timed sweep. So
**the walk needs no build; it needs a headset session.**

The plan the cycler builds is deterministic and already on that log, so the
recording sheet can be written now. Four positions, Numpad 3 stepping forward:

| position | component | what the engine calls it |
|---|---|---|
| 1 | `Skm_Player` | the first-person body, section 0, LOD 0 |
| 2 | `Wpn_PlySword01` | the sword |
| 3 | `crossbow_01` | the crossbow |
| 4 | `bolt_01` | the loaded bolt |

Numpad 1 steps back, Numpad 2 restores everything. Each press logs its position
and component, so the walk only needs the tester to say what vanished at each
one.

**One thing the census already settles, so the run is not spent on it:**
`Skm_Player` is the ONLY component carrying first-person body geometry. Arms and
hands cannot be on separate positions - there is nowhere else for them to be.
The question the walk actually answers is position 1: do both arms AND both
hands go, and do the weapons keep drawing? (ENGINE_NOTES, "What the cycler can
still settle".)

### Our own hands: the caller was the missing piece, and it is back

`core/gfx/hand_mesh.cpp` has drawn nothing since 41.0. `HmRenderEye` and
`HmEnsurePipeline` lost their only call site when the side-by-side pipeline was
deleted (`cc2fa936`), so `[VRHands] Enabled=1` changed nothing on screen and
logged nothing about it. Rebuilt this session:

- a `HandDrawFn` seam beside `OverlayDrawFn` in `core/gfx/stereo.h`; the active
  method calls it between the game image and the F10 panel, passing **the eye
  tag of the pixels already in the target** (not the eye the next game draw
  renders - one line apart in `reentry::end_frame`);
- the pass's own D32 depth buffer, sized to the method's output and cleared per
  draw. The painter's sort is gone; without a bound DSV the depth state is inert
  and the back of a hand paints over its front;
- **the frustum corrected to the projection layer's CLAIM** (the engine's
  rendered hfov, derived the way the runtime derives its own half-angles)
  instead of the headset's raw half-angles - 108.1 deg against the headset's
  numbers on the last measured run, and a slide against the world that grows
  with the gap;
- a refusal on the mono screen that says which rung would work, because a silent
  skip and broken hands look identical in a log;
- a 3 s beat printing calls / draws / triangles that **names the reason for any
  zero**, and `handModelTris` / `handModelWhy` in `status.json`.

**Ships ON** (`[VRHands] Enabled=1`) for the test sessions - the same deliberate
exception as `[Device] ShadowFullCopy` and `[Hands] BoneVisHide`, so a run shows
the hands without anyone sending a seam word first; it reverts to OFF when the
verdict is recorded. `vrhands on|off|status` is the live A/B.

**`[VRHands] HideGameArms` ships OFF with it, on purpose.** It collapses the
game's own view-model rigs by upload size (the 30.77 vs-const path) - a SECOND
behavioural change in the same build as the first draw of ours, against the
author's one-change-per-build rule. It also makes run 1 diagnostic instead of
pass/fail: with the game's arms still drawn, they are the reference our hands
are judged against. If our hands land right, that flag is the whole remaining
step to floating hands.

**UNVERIFIED - nothing here has been seen in a headset or on the simulator.**

### Next steps

1. Walk the cycler (4 positions above) and record position 1's answer.
2. Run 1 of the hands: nothing to switch on. Read the beat line. Expect
   the first run to need trim: `[VRHands] Scale`, the per-hand `PosX/Y/Z` and
   `Yaw/Pitch/Roll`, all already in the ini, none of them exercised since 30.x.
   The overlay panel for them was deleted in 31.5 and would have to come back if
   trimming by ini proves too slow.
3. Floating weapons as a shipping lever: hide `Skm_Player`, default OFF, live
   A/B, and check what it does to the shadow and to Blink's aim.
4. Routes (b) and (c) are not needed for either. (a) is closed.

## PREVIOUS (2026-09-06, session 18): VR-31 - route (d) WORKS, floating weapons are in reach

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`, not `Fixes VR-31`). Installed build `alpha-305`.

### PROVEN, headset-judged: ShowMaterialSection hides first-person geometry

Four automatic steps, geometry disappeared and returned on each. **Route (d)
works.** Full record in `ENGINE_NOTES.md`, "SOLVED: route (d) WORKS".

The plan the engine's own `GetNumElements` produced:

| step | component | sections |
|---|---|---|
| 1 | `Skm_Player` (the first-person body) | 1 |
| 2 | `Wpn_PlySword01` | 1 |
| 3 | `crossbow_01` | 1 |
| 4 | `bolt_01` | 1 |

**Every component has exactly ONE material section.** Route (d) therefore hides
per COMPONENT, not per body part.

- **This gives floating weapons now**: hide `Skm_Player`, and the sword,
  crossbow and bolt keep drawing as separate components.
- **It cannot give floating hands**: arms and hands share one section, so
  hiding the arms hides the hands too. No finer material cut exists in the
  asset, and none can be made from outside it.

**The tester's per-step attribution is NOT yet established.** The recollection
of which step removed which limb was approximate. The table above is the PLAN,
not what was seen. The numpad cycler exists to settle it deliberately.

### The cycler

`[Hands] MatCycle=1`, ships ON. **Numpad 3** next, **Numpad 1** back,
**Numpad 2** everything visible. Each press logs the component, section and LOD
it hid. The hotkey only posts a request; every dispatch runs on the SCRIPT
lane in `MatCycleTick`, because ProcessEvent from the present thread is the
lane error this project has a rule about. The timed sweep (`[Hands] MatAuto`)
is back OFF now the route is proven, so the two cannot fight.

### Next: floating hands needs a different SOURCE for the hands

Not a finer cut of the game's mesh - that split does not exist. The mod already
has its own: `core/gfx/hand_mesh.cpp` draws hand geometry and the SkelControl
drive already places hands from the controllers. Hide `Skm_Player`, draw our
own hands. That is the BioShock shape reached from the opposite direction, and
it also answers the powers requirement - our own hands can be shown for powers
and hidden otherwise without touching the game's mesh at all. Untested.

### Next steps

1. Walk the cycler and write down which component is which limb, so the
   attribution is measured instead of remembered.
2. Floating weapons as a shipping lever: hide `Skm_Player`, default OFF, live
   A/B, and check what it does to the shadow and to Blink's aim.
3. Floating hands: `hand_mesh.cpp` + the SkelControl placement, with a
   visibility policy that shows hands for powers.
4. Routes (b) and (c) are not needed for either of the above. (a) is closed.

## PREVIOUS (2026-09-06, session 18): VR-31 - route (a) closed, route (c) found

Branch **`claude/vr-31-arm-hiding-floating-hands`**, now cut from
**`claude/vr-30-decouple-arm-hand-movement`** (rebased 2026-09-06, session 18).
Linear **VR-31** is In Progress.

### The base, and what that means for the PR

This branch **carries the VR-30 fix** and is safe to build and install from. It
is based on the VR-30 branch, not on `VR-Main`, because VR-30 is still open in
**PR #18**. So:

- the PR body's first line is `Ref VR-31`, not `Fixes VR-31` - only the PR that
  reaches `VR-Main` closes the ticket, and this one will not until #18 lands;
- **PR #18 merges first.** After it does, this branch rebases onto `VR-Main` and
  its PR retargets there.
- The tester's ini carries `[Camera] ArmBodyFacing=1` (the VR-30 fix, on),
  `ArmStripMeshRot=-1` and a leftover `BodyYawLock=-1` whose key no longer
  exists in this code and is ignored.

### VR-30 is DONE (2026-09-05, session 17)

Headset-judged: head yaw leaves the arms and weapon alone, the right stick turns
view and body together, both at once works. `FaceRotation` is virtual at vtable
slot 252 (`0x00AB0D40`); the mod replaces the **Yaw it is asked for** with
`view - our own injected head contribution` and lets the engine's own function
run. Measured 3365 replacements, 0 stale, `perf: tick` unchanged at 11.3-11.8 ms
against an 11.11 ms budget. Full record in `ENGINE_NOTES.md`, "SOLVED: VR-30".

### VR-31: the question, and where the research already is

**The deliverable is a verdict, not a feature**: floating hands via the
bone-density trick, or floating weapons with hands only for the powers.

**The 30.13 result is NOT missing.** Session 18 found it. The original author
recorded it in a code comment in the state chunk beside the experiment
(`src/mod/state/40_game_dishonored_hands_arms_hide.inc`, and in the original
single file at `git show 824e08d8:src/dllmain.cpp` line 11119), which is why
three passes over the corpus called it unrecorded:

    30.14: the +0x288 byte poke was wrong - that array turned out to be some
    per-bone animation control (arms froze to the view and rode the head).

So the write was made, and it had a visible effect that was not hiding. The
probable reason: `0xFF` is not a legal `BoneVisibilityStates` value, but it is
exactly `SkelControlIndex`'s "no control on this bone", so 30.13 most likely
attached the arm bones to SkelControl #2 rather than hiding them. That also
weakens the existence proof route (a) rested on - the game may not be hiding
`spine_3_jnt` at all, it may be pointing it at a controller. Full record in
`ENGINE_NOTES.md`, "The 30.13 bone-visibility result was recorded all along".

**What session 18 built instead of repeating it.** The offsets do not have to
be guessed: this build ships UE3 reflection (`FindPropOffset` reads
`UProperty::Offset` out of GObjects, as the crouch cylinder and the graft
already do). `arms vis on|off|status|chain`, ini `[Hands] BoneVisHide`, **ships
ON for the test sessions** (a deliberate exception to "every lever ships OFF",
the same call as `[Device] ShadowFullCopy`, so a run prints the census without
anyone sending a seam word; it reverts to OFF when the verdict is recorded):

- asks the engine where `BoneVisibilityStates`, `SkelControlIndex` and
  `RequiredBones` really live and prints all three against `0x288`, naming
  which array 30.12 actually found;
- writes `BVS_ExplicitlyHidden` into the arm chain at the offset the engine
  named (never hands or fingers), saving and restoring the exact bytes;
- refuses, with numbers, if the property does not exist, if the array is empty
  (`num=0` - the engine never allocated it, which closes route (a) with a
  reason and is the same fact 30.17 saw), or if its length is not the rig's
  bone count;
- runs a write-survival census every 2 s while on - `held` / `reverted` /
  `other` - so "the write did not survive" and "the write survived and the
  renderer ignores this array" stop looking identical in a headset.

Route (b), the c6 bone palette (**x36 = sword, x144 = arms, x204 = NPC**,
machinery in `src/legacy/rtd_drive.cpp`), is untouched and is where an all-held
census sends the question next.

### VERDICT (2026-09-06, runs 1-4): route (a) is CLOSED, and route (c) is new

Four runs, all read-only, all with the arms staying visible - which was the
correct result each time: the lever refused to write and said why.

**Route (a), per-bone visibility, is closed on three independent instruments:**

1. `BoneVisibilityStates` does not resolve on `SkeletalMeshComponent`
   (`RequiredBones` resolved at `+0x23c` in the same call, so the resolver was
   working and the `0` is a real absence);
2. no `UProperty` of that name exists on **any** class in GObjects, so it is
   not hiding on a subclass;
3. of the five per-bone byte arrays on the rig, none has its values confined
   to `0..2`, which is what a visibility array must look like.

**`+0x288` is `SkelControlIndex`, measured.** 30.13 pointed the arm bones at
SkelControl #2 rather than hiding them, which is exactly the symptom 30.14
recorded. The existence proof route (a) rested on is gone with it: the game is
not hiding `spine_3_jnt`, it is pointing that bone at a controller. And 30.17
is consistent with it - `HideBoneByName` allocating nothing is what a missing
array would produce - but the native's implementation was never inspected, so
that is the likeliest reading and not a demonstrated cause.

**Route (c) appeared in the same run.** `SkelControlIndex` on the arm chain:
**8 of 10 bones read `255` (free)**, and the 2 that are taken are exactly the
two collarbones, driven by controls **0** and **1**. One byte per bone, and the
mod already drives SkelControls. What those two controls are is not yet known -
the mod's own control enumeration was off this run (`[Hands] Enabled=0`), so
naming them is one cheap run with the hand drive on.

Full record, including the scan's element-size caveat, in `ENGINE_NOTES.md`,
"VERDICT (2026-09-06, run 4)".

### The three routes, as they now stand

| route | state | cost | unknown |
|---|---|---|---|
| (a) per-bone visibility | **CLOSED**, three instruments | - | none, it is finished |
| (b) c6 bone palette | **OPEN**, proven writable | the hottest D3D9 entry point | none material: x36 = sword, x144 = arms, x204 = NPC, machinery in `src/legacy/rtd_drive.cpp` |
| (c) `SkelControlIndex` | **OPEN**, new | one byte per bone | what a free bone can be pointed AT - the index selects from the AnimTree's control lists |

**The lever is back to default OFF** now the verdict is recorded. What is kept
is the diagnostic that closed the route: `arms vis status` re-runs the whole of
it on demand, `arms vis chain` prints the arm chain.

### Next steps

1. **The route decision is the user's**, and it is the real open question. (b)
   is the safe one - proven writes, mesh separation already measured, and the
   only doubt is frame cost on the D3D9 hot path. (c) is the cheap one and the
   more interesting one, but it needs the AnimTree control list understood
   before a single byte is written, and 30.13 is the standing warning about
   pointing bones at a control somebody else owns.
2. Either way, one cheap run with `[Hands] Enabled=1` names SkelControls 0 and
   1 (and settles whether #2 is `LookAtControl_Camera`, which would close the
   30.13 story completely).
3. PR is not open yet. It takes `Ref VR-31`, not `Fixes VR-31`, while the base
   is the VR-30 branch.

### Session log

### 2026-09-09 - VR-66: the stale command line was the mod's own file

The render size had two homes and one writer. `dishonored_vr_launch.txt` carries
`-ResX/-ResY` and is read in `DllMain`, before the engine's entry point - that is
the route the engine obeys. `[Screen] RenderWidth/Height` in the mod ini is read
much later at `EnsureConfig` and drives the mode `VirtualMode` advertises. Both
were only ever written together by `ResRequest`; a text editor writes one.

So the two failed attempts asked with a text editor, moved the advertised mode
to 3200x3300, and left the engine being told 2750x2850 - the file's value from
five days earlier, still on disk with that timestamp. The engine asked for a
size that was no longer in the mode list and UE3 fell back to a real display
mode, which on this monitor is 2560x1440. **Neither ini ever contained 2560x1440;
the mod supplied it.**

`[Screen]` is now the authority. The ask is resolved from it on the engine's
first `GetCommandLine` call - the CRT startup glue at the exe's entry point, past
the loader lock, so the ini read `DllMain` is forbidden to do is safe and still
early enough. One resolved ask sets both the command line and the advertised
mode, so they cannot diverge; a disagreement logs both values and rewrites the
file; and the hooks now install with no launch file at all, which an ini-only
ask previously needed and never got.

**Two lessons, both already in this file in another form.**

*An ini key that exists beats every compiled default* (VR-65) has a sibling: a
file that exists beats the ini you edited. Anywhere one setting has two
persistent homes, the one nobody thinks to edit is the one that wins.

*Every refused guard says why, with the values.* The `CreateDevice` mismatch
warning named the game's own ini - the route measured inert on this build - and
did not name the command line, which is the route that decides. It now prints
what the engine was handed, how many import slots were patched, and whether the
size the engine wanted is one of the adapter's real modes, which is the fallback
signature.

**Not verified.** No run at a raised size has happened. The mechanism comes from
the logs and the launch file's timestamp.

### 2026-09-07 - VR-33: the hands reach the controllers

**Verified in the headset:** hands track the controllers, are correctly scaled,
occlude against world geometry, and hold position through head turns. PR #21.

**The cleanup is verified too**, after a scare worth recording. The trim did not
compile: `g_mpEyeHunt` lost its declaration and the capture packet still
referenced two retired modes - but MSBuild was linking STALE OBJECT FILES, so
three builds reported success while the DLL kept an older build id. Comparing
the id embedded in the installed DLL against `git describe` is what caught it;
a "clean build" had been meaningless. Fixed in `fe531095`, rebuilt with a
forced recompile, and confirmed in the headset with the ids matching.

**What the session established.** The bone palette's output is the component's
LOCAL space; `LocalToWorld` (c231) maps it to a camera-relative world frame and
`ViewProjectionMatrix` (c0) to clip. Register indices differ per shader and are
parsed from each shader's CTAB - three shaders draw this mesh and one defines c4
as an immediate that disagrees with the device. The shader does not normalise
skin weights. The view model is depth-crushed to MaxZ 0.001. The eye separates
Present-to-Present in LocalToWorld's translation by 6.76 uu against a predicted
IPD of 6.31.

**What was falsified**, each with the measurement that killed it: a truncated
GObjects census reporting its array capacity as a population; a relative drive
whose cancellation argument omitted the animated hand underneath it; a
head-space neutral; a yaw residual, killed by phi holding constant at 144 deg
across a 145 deg head swing; a calibrated origin that became a 1.4 m lever on
the head; and three eye classifiers - a game-thread flag that reads false on the
render thread, a learned midpoint that was not head-invariant, and an ordinal
that sampled per HAND so its pair was one draw compared with itself.

**Instrument lesson, and the reason the above cost so much.** Three instruments
produced confident readings later withdrawn, and once an absence was reported
that had never been established. All four survived because they were checked
against expectation rather than against their own ability to fail. The rule
adopted: an instrument must declare and log the unit it sampled, and "nothing
was sampled" must never be able to read as "no difference was found".

**Seen and not chased:** each shader draws the mesh three times per frame, and
only one pass is depth-crushed. The purpose of the other two is unknown.


**2026-09-06, session 19 (part 3)**: floating hands WORK, using the game's own
hands. Built the draw census, then the mesh lock (buffer-pair identity, both
draw entry points), then live triangle-range slicing - the mesh is a triangle
list, so sub-ranges of the game's own index buffer can be drawn without reading
or replacing anything. Two headset screenshots confirm real hands with the arms
gone. The tester's cut is `0x3001D237`, restorable with `dc mask s19`; it is not
a default because the two arms sit in non-aligned regions and it was tuned on
the left. Also found and fixed: single-matrix draws were saturating the census
table, and a full table silently disabled suppression - which was the whole
explanation for the faint arm that survived earlier hides.

**2026-09-06, session 19 (part 2)**: the cycler walk came back and matches the
engine's plan exactly - arms+hands together, then sword, crossbow, bolt. VR-31's
route (d) question is answered. The hands showed nothing despite healthy
counters; review found `RtdBuildYPR` is a legacy-only symbol that resolves to an
empty stub in every shipped build, so the hand transform ran on an uninitialised
matrix. Replaced with a local `HmBuildYPR` (verified identity at zero and equal
to Ry*Rx*Rz to 2.2e-16), separated `submitted` from `ON-SCREEN` in the beat,
fixed the constant-0.5 depth, and added a calibration triangle as the fallback.
Swept all 23 legacy stubs: exactly one was unsafe. Corrected an error in part 1's
write-up - the stereo tagging claim was read off bring-up beats and gameplay is
properly tagged.

**2026-09-06, session 19**: no headset run. Established that the cycler build
(`alpha-305-g23ef0ae5`) is the INSTALLED proxy and has never been run - the
newest log on disk is `alpha-304`, the timed-sweep run - so the walk needs a
session, not a build, and wrote the 4-position recording sheet from the census
already on that log. Then found that `core/gfx/hand_mesh.cpp` has been dead code
since 41.0: `HmRenderEye` and `HmEnsurePipeline` lost their only call site when
the side-by-side pipeline was deleted in `cc2fa936`, so `[VRHands] Enabled=1`
changed nothing and said nothing. Rebuilt the caller as a `HandDrawFn` on the
stereo seam, gave the pass its own depth buffer, corrected its frustum to the
projection layer's CLAIM rather than the headset's half-angles, made the mono
screen refuse out loud, and added a beat that names the reason for a zero.
Builds clean, lint clean, exports OK, installed Release. **Nothing verified** -
no headset, no simulator run.

**2026-09-06, session 18**: opened. Branch cut, VR-31 moved to In Progress,
research located and extracted. Found that the 30.13 experiment's result was
recorded after all and that `+0x288` is very likely `SkelControlIndex`, not
`BoneVisibilityStates`; corrected two dead-end entries in `ENGINE_NOTES.md` and
added the record. Built the reflection-resolved route (a) lever and its
write-survival census, default off. Builds clean, lint clean, exports OK.
Rebased onto the VR-30 branch and installed Release. **Four tester runs**:
route (a) is closed on three instruments, `+0x288` is `SkelControlIndex` by
measurement, this build has no `BoneVisibilityStates` anywhere, and the arm
chain is 8-of-10 free in `SkelControlIndex` - which is route (c). Two of the
four runs were spent on instrument preconditions I got wrong (the scan needs a
live rig, and the candidate list does not exist unless the hand drive is on);
both are recorded in ENGINE_NOTES so the next instrument does not repeat them.
Lever back to default OFF, diagnostic kept behind `arms vis status`.

**2026-09-05, session 17**: VR-30 solved and closed. PR #18 open against
`VR-Main`, unmerged. Also filed **VR-52** (place the hands absolutely from the
controllers) with the anchor bug and the acceptance direction recorded, since
that is a separate problem from this one.

## PREVIOUS (2026-09-05, session 17): VR-30 IS SOLVED - FaceRotation is the seam

Branch **`claude/vr-30-decouple-arm-hand-movement`**, cut from `VR-Main`.

**Headset-judged fixed.** Head yaw leaves the arms and weapon alone, the right
stick turns view and body together, and both at once works. That is VR-30's full
acceptance, both halves.

### The fix, in one paragraph

`FaceRotation` is what faces the body to the view. It is **virtual at vtable slot
252** and resolves to **`0x00AB0D40`** on this build. The mod hooks it and
replaces the **Yaw it is asked for** with a separated body heading
(`view - our own injected head contribution`), then lets the engine's own
function run so its dependent bookkeeping still happens. Pitch, roll, delta time
and every other actor pass through untouched. `[Camera] ArmBodyFacing`, ships
**OFF**, `arms facing 1|off` is the live A/B. Measured: **3365 replacements, 0
stale**, 1014 calls on other pawns untouched.

Full derivation, the falsified attempts with their numbers, and the identity bug
are in `docs/dishonored/ENGINE_NOTES.md`, "SOLVED: VR-30".

### What was removed, and why

- **`[Camera] BodyYawLock` is gone.** It wrote the pawn's `Rotation.Yaw` every
  dispatch and was measured futile: **4 of 186 writes survived** to the next
  dispatch. Its target was correct; the engine simply re-derives the pawn's
  heading from the controller every tick. Users with the key in their ini can
  delete the line; it is ignored.
- **The per-dispatch `FaceRotation` name match is gone** from `PeHandler`. Its
  question is answered (0 dispatches across a full run - the route is
  native-to-native) and the answer is in ENGINE_NOTES.

### What is kept as instruments

- `armfollow/yaw:` - the four-way yaw census (head, controller, pawn, view with
  per-second deltas). It found the stale-controller bug and the write-survival
  number. Now on the 1-in-128 slow path.
- `armfollow/nfp:` - the one-shot native-facing probe that derived the vtable
  slot. Costs nothing after it fires.
- `[Camera] ArmStripMeshRot` - ships OFF, kept as the reproducible A/B for a
  documented dead end (304,244 writes, no effect).
- `tools/yawtest-host.ps1` - compiles the yaw bookkeeping out of `head_track.cpp`
  **verbatim** and runs its seven cases on the host. No game, no headset.

### Performance

`perf: tick` on the winning run reads 11.3-11.8 ms against an 11.11 ms budget at
90 Hz, unchanged from before the branch. The hook adds a handful of integer
compares to a function that runs about once per frame per pawn. The census's
clock read was moved to the slow path (c5ecea34's rule); the answered
per-dispatch name match was deleted.

### Next

VR-30 is done. The hands are still placed by the engine - **absolute controller
placement is a separate ticket**, and `ENGINE_NOTES` records that SkelControl
world-space translation is absolute (attempt 5's pin test) and that the world
block's anchor is wrong: `camera+0x80` is not a position (DISCARDED 120/120),
`camera+0x330` is.

## SOLVED (2026-09-05, VR-15): the black texture bug was a MIP fault, and it ships fixed

PRs #14 (the crouch fix) and #15 (the 90 Hz defaults) are **merged to `VR-Main`**. Branch
`vr-15-black-texture` carries the fix, headset-judged by the tester.

**The fault**: a surface was largely black far away and correct up close, the black
receding as the player walked in. Distance selects the MIP LEVEL, so the bad data was in
the small mips and level 0 was fine.

**The cause**: the managed-pool shadow pushed every write with `UpdateTexture`, which
**takes no level**, and writes to levels above 0 were not being carried. `shadow_unlocked()`
was not even given the level - the unlock hook had it in hand and dropped it - so no
instrument could have seen a per-level fault. The 2026-09-04 log had the corroboration
sitting in it unread: **`level>0=50189`** locks in one load with **`dirtyRects=0`**.

**The fix**: `[Device] ShadowFullCopy` pushes exactly the level the unlock wrote, with
`UpdateSurface`, which names its two surfaces and cannot be vague about which level it
copied. It **ships ON** - a deliberate exception to "every render lever ships OFF", the
same call as `[Stereo] HoldUntagged`, because OFF is a visible rendering bug.
`device shadowfullcopy off` restores the fault and is the A/B.

### The frame-rate cost, and what was done about it

The first cut made the frame rate less stable. Three causes, two of them **older than this
session's work**:

1. **The push ran on READONLY unlocks** - 12408 of them per load, each a whole-texture GPU
   copy for a lock that wrote nothing. Now skipped; the lock kind is recorded in the
   lookup the lock hook already had to do, so it costs nothing to know.
2. **A refused `UpdateSurface` was re-attempted on every unlock forever**, paying for the
   failure and the fallback both. A refusing texture is now remembered.
3. **The 60 s upload census walked all 32768 twin-map slots on the present thread** just to
   decide whether to print. Self-inflicted this session; the decision now reads counters.

`shadow_unlocked` also took its critical section twice per unlock and now takes it once.

**Unmeasured**: nobody has put a number on the improvement. `perf: tick` before and after
is the measurement, and the `device/upload` line's "work skipped" count says whether the
READONLY skip is firing at all (a 0 on a loaded level means it is not).

### Still open on this branch

`[Device] ShadowSurfaces` (ships OFF) covers a different hole: the game locking a SURFACE
taken off a texture, which the shadow's redirect cannot see by construction. **Its hooks
have still never executed** - no run has exercised them. The `device/upload` line says
whether that path is used at all.

Mechanism and the full reasoning are in `docs/dishonored/ENGINE_NOTES.md`, "SOLVED: the
black texture bug".

### What was NOT done, and why

**The simulator run did not happen.** Two launch attempts: the first died in a C++ runtime
error before the D3D9 device was created (the mod fell through to the system's Virtual
Desktop runtime rather than the simulator, `[VR] XrRuntimeJson` being empty), so none of
the new hooks executed and the run says nothing about them either way. The second attempt
was stopped. **So the new vtable patches - `GetSurfaceLevel` 18 on textures and cube
textures, `LockRect`/`UnlockRect` 13/14 on surfaces - have been compiled and installed but
never executed.** Treat the first run with this build as a bring-up test: if the game dies
at its first texture, those slots are the suspect and `device/census` will say whether the
surface hook installed at all.

## CONFIRMED ON A THIRD RUN (2026-09-04): 2750x2850 at 90 Hz is the default and it holds up

Third headset run on the shipped defaults: smooth, weapon aligned, no ghosting, and no frame
rate below 80 observed. The 90 Hz result reproduces. The defaults now
carry it (`[Screen] RenderWidth=2750 RenderHeight=2850` plus the 90 Hz note beside them).

**One open issue on that run, and it is NOT new**: the session flickered for roughly 25 seconds
at the start before locking. It normally lasts a few seconds; this run it ran long, which is
what made it measurable for the first time. Diagnosed below, **not fixed** -
the first thing to try is a lever that already exists and has never been judged.

### The startup flicker: one eye starving while the level streams

`stereo: beat` across the run tells the whole story:

| t (s) | out/s | L/s | R/s | none/s | draws/s | |
|---|---|---|---|---|---|---|
| 8.5 - 17.5 | 21 - 85 | 0 | 0 | 0 | - | menu/loading, mono by design |
| **20.5 - 38.5** | 87 - 91 | **12 - 19** | **52 - 73** | 10 - 17 | **51 - 72** | **starved: the flicker** |
| **44.5 onward** | 180 | **90** | **90** | **0** | **90** | **locked, stays locked** |
| 77.5+ | 155 - 234 | 0 | 0 | 0 | - | pause screen, mono by design |

`perf: tick` in the starved window reads **17.5 ms against the 11.11 ms budget**, split
`P1[-1] n=36` against `P2[+1] n=156` with `untagged 107`. `reentry: beat` shows pass 2 running
throughout (`2nd/s == draws/s`, all skip counters zero), so the second draw is not missing -
**the game is simply producing 51-72 ticks/s against 90 display slots/s.** Below the display
rate the pair schedule cannot land one pair per slot, the tag stream goes lopsided, and 1016
same-eye pushes accumulate (always `+1`, so LEFT is the eye that starves). One eye refreshing
at ~18 Hz beside one at ~73 Hz is the flicker, and it looks like alternate-eye rendering
because structurally that is what it is.

**Same root as the ghosting, at a different ratio.** Tick slightly over the period gives the
beat (doubled edges); tick far over gives eye starvation (flicker).

### The fix theory, in order, and NOTHING here is implemented

1. **`vrpace strict on` first.** It already shows the fresh eye to BOTH eyes when one is stale,
   which converts the starved window from alternating eyes into a briefly flat picture. It
   ships off, toggles live, and has never been judged. **Try this before any code is written.**
   It wants an A/B rather than a default flip, because it will also fire on the rare
   mid-gameplay stale eye and cost depth for that frame.
2. **If that is not enough**, the shape of a real fix is to extend the `HoldUntagged` idea from
   untagged presents to unbalanced pairs: hold the previous good pair rather than submit a
   lopsided one, bounded so a permanent hold cannot freeze the image.
3. **Measure before either**: why LEFT specifically. Hypothesis - the shared-capture deferred
   delivery (`SharedWait=0` hands over the PREVIOUS slot) repeats a tag when presents arrive
   irregularly. `capture sharedwait on` is the A/B that tests it. This is a hypothesis, not a
   measurement.

## NEXT SESSION (2026-09-05): make the startup phases hook instantly

**The goal**: a load should come up in stereo, aligned, immediately. Today it walks through
mono, then the arms hook and reposition, then stereo hooks, then the weapon and hands flicker
until they lock - **15-25 seconds of settling, every load.** The 2026-09-04 run measured 26 s.

### The measured startup timeline (from the s15c log, times from proxy load)

| t | what happens |
|---|---|
| 0.00 s | pad IAT hook; the engine command line is extended (`-ResX/-ResY`) |
| 0.6 s | config read: hands, hand render drive, crouch, crash handler |
| 0.86 s | `res` adapter-mode hooks installed |
| **3.11 s** | **device hooks** (Present/Reset/SetVSConstF/SetRenderTarget/BeginScene) + the creation census hooks |
| 4.86 s | `[game] state: NO_PAWN` |
| **5.08 s** | XR session live; `reentry: ARMED` (call site patches at the next script dispatch); **`blockhunt: walking 65821 objects`** |
| 17.1 s | `[game] state: MENU` |
| **18.4 s** | `[game] state: GAMEPLAY`; 14058 D3D creations logged at first GAMEPLAY |
| **20.5 s** | stereo tags start - but **starved** (L/s 12-19 against R/s 52-73) |
| **44.5 s** | **locked**: L/s = R/s = 90, nothing untagged, and it stays that way |

**So the settle is two separate problems and they should not be conflated:**

1. **0 -> 18.4 s is mostly the GAME loading**, not us. Our own hooks are all in by 5.1 s. The
   only clearly-ours cost in that stretch is `blockhunt` walking **65821 UObjects** at 5.08 s -
   worth timing before assuming it is free, and an obvious candidate for caching its results
   (the offsets it finds are build-constant) or deferring it off the critical path.
2. **18.4 -> 44.5 s is the eye starvation, and it is OURS to handle.** Diagnosed in the section
   above: while the level streams the tick runs 51-72/s against 90 display slots/s, so the pair
   schedule cannot land one pair per slot and one eye starves. **This is the 26 seconds the
   player actually sees**, and it is where the work is.

### Where to start, cheapest first

1. **`vrpace strict on`** - already exists, never judged, one command. It should turn the
   starved window from alternating eyes into a briefly flat picture. **Do this before writing
   any code.**
2. **Measure why LEFT starves specifically.** Every doubled push is `+1`. Hypothesis: the
   shared-capture deferred delivery (`SharedWait=0` hands over the PREVIOUS slot) repeats a tag
   when presents arrive irregularly. `capture sharedwait on` is the A/B that tests it. This is a
   hypothesis, not a measurement.
3. **Then consider holding unbalanced pairs**, extending the `HoldUntagged` idea: hold the last
   good pair rather than submit a lopsided one, bounded so it cannot freeze the image.
4. **Separately, time the startup hooks themselves.** There is no instrument that says how long
   each phase took - `blockhunt`, the census hooks, the reentry call-site patch, the first
   GAMEPLAY transition. Without that, "make startup instant" has no scoreboard. A phase-timing
   line is probably the first thing to build.

**Do not re-open**: the ghosting (solved, cadence, 90 Hz), the bbox readback (gated, prediction
falsified), motion blur (already off), a per-eye tag asymmetry (impossible - the pair shares one
locate). The 60 fps dips are the Wi-Fi encoder, not the frame path.

## SOLVED (2026-09-04): the GHOSTING was the cadence beat, and 90 Hz is the fix

**No ghosting reported at 2750x2850 on a 90 Hz headset.** Same build, same scene, same render
size at 120 Hz: ghosting still reported. One setting changed.

| | 120 Hz | 90 Hz |
|---|---|---|
| display period | 8.33 ms | 11.11 ms |
| `perf: tick` p50 (p90, max) | 9.1 ms (10.8, 12.9) | 11.3 ms (12.0, 15.1) |
| **display slots per frame** | **1.05 - 1.11** | **1.00 - 1.02** |
| EVEN / UNEVEN windows | 9 / 20 | **33 / 16** |
| MATCHED / UNDER-SUBMITTING | 11 / 26 | **38 / 22** |
| ghosting reported | yes | **no** |

At `off` slots of drift per frame, one frame in `1/off` is held for an extra display slot, and
consecutive frames shown for different durations IS the doubled edge. At 1.11 that is every 9th
frame; at 1.01 every 100th. **The fault was never the resolution and never the pose
attribution - it was the tick not dividing into the display period.**

**The defaults now carry it**: `[Screen] RenderWidth=2750 RenderHeight=2850`, with the 90 Hz
requirement written into the ini text beside it, because the pair is one setting and the refresh
half lives in Virtual Desktop where no ini can reach it.
`tests/golden/known-good-2750x2850-90hz.ini` is the byte copy of the machine that was judged.

### Session 14's falsification was WRONG, and the threshold was why

Session 14 measured 1.03-1.05 slots per frame, read `EVEN CADENCE`, and closed the cadence
hypothesis. The verdict was lying: its threshold was `|off| > 0.06`, so it called 1.05 - a beat
every twenty frames - clean. **The hypothesis was right and the instrument's threshold was
wrong.** Now 0.02, drawn at the measured edge (1.02 does not ghost, 1.05 does), and both
branches print the beat as a number so an "even" verdict shows the residual it forgives.

### Why it drops to 60 at 90 Hz when it never dropped below 90 at 120 Hz

Not a contradiction, and the hitch RATE did not change - normalised by run length it is
**27.6 gaps/min at 120 Hz and 28.4 at 90 Hz. Identical.**

- At **120 Hz** the tick never fit the 8.33 ms slot, so the app never tried to hit one. It
  free-ran and the compositor smeared over the mismatch. No cliff to fall off when you are
  already past the edge: the rate reads a smooth 100-120 and the ghosting is constant.
  **Smooth, and always wrong.**
- At **90 Hz** the tick sits right at the 11.11 ms period. Most frames make their slot - which
  is what removed the ghosting - but one that misses waits a whole period, so an 11.3 ms
  overrun displays for 22.2 ms (45 fps instantaneous) and a run of them averages toward 60.
  **Correct, with a cliff directly underneath.**

The stalls were always there; they are just visible now, standing out against a locked cadence
instead of disappearing into a permanently smeared one.

### What is actually causing the remaining drops, and it is not ours

**54 of the 71 gaps sat in `present-tail (xrEndFrame)`, blocking up to 101 ms.** On a Wi-Fi
streaming runtime a 101 ms block inside the submit call is the encoder or the link. Next steps,
cheapest first, all on the Virtual Desktop side: raise the bitrate or change codec, check the
link speed and channel, try a wired/dedicated AP. Only after that is ruled out is it worth
looking at our frame path again.

The other lever, if you want margin instead: buy ~1 ms of tick. At ~0.63 ms/MP a step to about
**2600x2700** (7.02 MP) predicts ~10.3 ms against the 11.11 ms period - real headroom under the
cliff, at a small sharpness cost. Untested.

### Falsified, honestly: the content-bbox gate was not the hitch cause

Session 15 predicted that gating the 3-second full-frame readback would cut the `perf: frame
gap` count by roughly the number of 3-second windows. **It did not.** Samples fell from one per
3 s to 2-3 per run; the gap rate was unchanged. The counter-evidence recorded next to the
prediction - the gaps sat in `xrEndFrame`, not the capture phase - was the correct read. The
gate stays because it removed a real ~30 MB present-thread stall for free, but it did not fix
what it was predicted to fix.

### The pose-lane instrument: validated, still unarmed

`xr: poseaudit SEAM CHECK ok - the script lane's yaw reads 20.67 deg and this file's own
converter reads 20.67 deg for the same head pose (0.00 apart)` fired in **both** runs. The sign
calibration is proven correct against live data, so a delta it prints would be a real
disagreement. **Nobody armed it** (`vrpace poseaudit on`), so the pose-attribution question is
still open - but it is no longer the ghosting's leading suspect, because the ghosting is
explained. Keep it for the judder/`ahead` work.

## SUPERSEDED (2026-09-04): the pose-lane instrument was built for a fault the cadence explained

Everything below is **built, linted, installed and unverified at runtime** - nothing has been
launched. What the next headset session does, in order:

1. **Set Virtual Desktop to 90 Hz.** This is not optional decoration, it is the arithmetic.
   Fitting the three measured ticks against megapixels (4.56 MP -> 8.75 ms, 6.71 -> 9.55,
   15.73 -> 15.7) gives **~0.64 ms per megapixel on a ~5.6 ms fixed floor**. The floor is the
   game's own CPU tick plus two presents and resolution does not touch it, so at 120 Hz the
   entire 8.33 ms budget leaves 2.7 ms of GPU for two full-frame scene draws - unreachable at
   any VR-useful size. **90 Hz (11.1 ms) is the honest target.** 2750x2850 is 7.84 MP and
   predicts a **~10.6 ms tick**; if it lands far off that, the fit is wrong and say so.
2. **Launch.** 2750x2850 is already armed in all four places (`tools\arm-res.ps1 -Status`
   shows them). The log must read, in order: `res: launch: command line extended ...
   -ResX=2750 -ResY=2850`, `res: handed the game our 2750x2850@<hz> mode`, `CreateDevice - the
   game asked for 2750x2850`, `capture: 2750x2850`, `res: HONOURED`, `xr: swapchain pair
   2750x2850`.
3. **Reach gameplay under `stereo reentry`, then `vrpace poseaudit on`, and turn the head.**

### The pose-lane instrument - what it answers and how to read it

The mod samples the head twice: the SCRIPT lane drives the game camera (the pose the pixels
are DRAWN with), and the PRESENT lane tags the projection layer (the pose the compositor
reprojects FROM). If they disagree the warp is wrong by the difference every frame, worst when
turning fastest and worse when a frame is slow - which is the reported percept exactly. Nobody
had ever measured it. Now:

```
xr: poseaudit SEAM CHECK ok - ...                      <- must appear FIRST
xr: poseaudit L tag .. R tag .. vs SCRIPT-lane .. -> delta L +x.xx R +x.xx deg
    | rendered sample is N locate(s) back, tag is lag=1 -> GENERATION GAP +N
    | one generation costs X.XX deg at this head speed | sample age .. ms ..
```

- **SEAM CHECK first.** The two lanes read yaw out of the same matrix with opposite sign
  conventions (`atan2(m02,m22)` vs `atan2(-m02,m22)`), so a naive comparison would read twice
  the yaw and look like a catastrophic fault that is purely convention. The seam negates once
  and then proves it against live data. **If that line says FAILED, every delta after it is
  meaningless - stop and report it.**
- **`GENERATION GAP 0` with a delta near 0.00 kills the hypothesis** and that is a real result,
  written down here in advance so it cannot be explained away later.
- **A steady nonzero gap names the fault in whole generations**, and `vrpace lag 0|1|2` is the
  live A/B: one of the three must null it.

**The written prediction: the gap is +1 and `vrpace lag 2` nulls it.** The tagging code assumes
"locate N feeds the tick that presents at N+1", one generation, which is why `lag` ships at 1.
But the game's own `DishonoredEngine.ini` carries **`OneFrameThreadLag=True`** - UE3's render
thread runs a frame behind the game thread, so the pixels in present N were drawn from locate
**N-2**. If `lag 2` fixes the ghosting, `OneFrameThreadLag=False` is the independent second
test: it removes the skew at the source instead of compensating for it, at a throughput cost.

### Also shipped: the 3-second stall nobody had looked at

`capture.cpp`'s content-bbox instrument (the `FULL`/`CROPPED` line) needs CPU pixels, and in
the shipping `shared` mode - whose whole purpose is that nothing goes to the CPU - each sample
is a full `GetRenderTargetData` + `LockRect` + row copy of the entire frame **on the present
thread**. That is the same round trip that costs 17-21 ms/present in `sync` mode, it ran every
3 seconds, and there was no lever. `[Capture] BboxMs` now defaults to 30000 with
`capture bbox off|<ms>` live; a size change still resamples immediately.

**Falsifiable prediction:** if this is behind the hitches, the `perf: frame gap` count should
fall by roughly the number of 3-second windows in the run (62-82 gaps over the last two runs is
close to one per window). **Counter-evidence already on record:** those gaps mostly reported
`sat in: present-tail (xrEndFrame)`, not the capture phase. If the count does not move, this
removed a real cost and was not the hitch cause - say that.

### Two more suspects died without a run

- **Motion blur.** Already off: `MotionBlur=False` and `MotionBlurPause=False`, with Arkane's
  own comment in the file - "Motion blur is unwanted". Not the ghosting.
- **A per-eye tag asymmetry.** Under `reentry` the LEFT present holds the XR frame open and the
  RIGHT completes it; `on_present_begin` returns at the top while a pair is open, so there is
  no second waitFrame and no re-locate between the eyes. **Both eyes of a pair share one locate
  generation.** The instrument prints per eye anyway so the invariant is checked, but do not go
  hunting this.

### Still not done from the tester's list

FXAA (`iType_AntiAlias` 1 -> 2) and the vsync A/B are **not** wired - the real keys and the
AppCompat requirement are recorded below and unchanged. Killcam is still not found in any ini.

## OPEN (2026-09-04): the GHOSTING - the cadence was NOT the cause, and that is now measured

**The report** (tester, on `alpha-264-ge2b84a80` at 2064x2208): "still ghosting flicker when turning
head and sometimes it hits harder than others".

### The resolution lever WORKS. Proven, because the tester doubted it and was right to

Every number moved, and the log names the mechanism at each step (`res: handed the game our
2064x2208@240 mode (slot 112)`, `CreateDevice - the game asked for 2064x2208`, `capture: 2064x2208`):

| | 2496x2688 | 2064x2208 |
|---|---|---|
| `perf: tick` | 9.2-9.9 ms | **8.6-8.9 ms** |
| submits/s (of 120 slots) | 88-115 | **114-117** |
| supply verdict | UNDER-SUBMITTING 0.73-0.96x | **MATCHED 0.95-0.97x** |
| slots per frame | 1.13 | **1.03-1.05** |
| interval sd | 1.3-3.8 ms | **0.75-1.36 ms** |
| cadence verdict | UNEVEN | **EVEN CADENCE** |
| capture lock | 0.5 ms | 0.0-0.1 ms |

### And the ghosting SURVIVED it. The cadence hypothesis is falsified

This is the point of having written the prediction down. The cadence is now locked - EVEN CADENCE,
1.03-1.05 slots per frame, sd under 1.4 ms, submits MATCHED to the slot rate - and **the doubled
edges are still there**. So the interference beat was real, was measured, was fixed, and **was not
what the tester is seeing.** Do not spend another session on pacing for this symptom.

What that leaves, cheapest first, all live commands with no relaunch:

1. **Virtual Desktop's Synchronous Spacewarp.** It manufactures intermediate frames by
   reprojecting, which is a literal ghost-frame generator, and it engages and disengages on its own
   - which is what "sometimes it hits harder than others" sounds like. Turn it OFF in the VD
   streamer settings and repeat the same head turn. **This is the first test and it is not ours.**
2. **`vrpace lag 0|1|2`** - which locate generation the layer's views are tagged with (ships at 1,
   "one back"). Infinite recorded the identical open suspect for the identical percept: "camera
   movement feels 'a bit jumpy' beyond the hitches - candidates: ... **the one-pair-stale
   content-pose attribution**". If the tag is a generation off the pose the frame was actually
   rendered from, the compositor reprojects by the wrong amount and the error changes every frame -
   doubled edges under rotation, worst when turning fastest.
3. **`vrpace ahead 0|1|2`** - the locate TIME (ships at 0). Already on the carried list as an
   unjudged judder item. The pair phase reads a steady **-43 ms** (we close ~5 display periods
   before the slot we asked for; on a Wi-Fi streaming runtime that is VDXR's pipeline depth, not a
   fault), so the reprojection is doing 40+ ms of extrapolation on every frame and the pose it
   extrapolates FROM has to be right.

### Not faults, checked so nobody re-checks them

- **The `CROPPED` capture bbox is a menu/loading artifact, not the resolution change.** Both runs
  show `100% x 52-53% (CROPPED)` early and then `100% x 100% (FULL)` once gameplay starts. Same
  shape at both sizes.
- **Not frame duplication or eye desync**: `mono/s=0 none/s=0-1`, `L/s == R/s == out/s / 2`,
  `ageL=1 ageR=0`, `aborts=0 staleEye 0` throughout gameplay.
- **Not Infinite's 30-second GC grid**: its spike class was
  `TimeBetweenPurgingPendingKillObjects=30`, killed A-B-A with 300. Our gaps have no 30 s
  periodicity - 0-13 s in bursts. Infinite's other signature (the streaming / level-visibility walk,
  triggered by view change and traversal) is the closer match and matches the head-turn trigger.
- **The hitches did not improve with resolution**: 62 gaps at 2496x2688, 82 at 2064x2208. They are a
  separate, later item from the ghosting.

### The 4K run: the lever is proven twice over, and SSW is OUT

**3840x4096 ran.** `perf: tick` **15.6-15.8 ms, 64 ticks/s** against 8.6-8.9 ms at 2064x2208 - the
lever moves the cost by 1.8x, exactly as pixels predict, so it is not inert and never was. Tester:
"4k does look way better". The capture lock also scales (0.0-0.1 ms -> 1.4-1.7 ms), which is the
readback and is ours.

**Virtual Desktop's SSW was already OFF** (tester confirmed), so suspect 1 of 3 is dead without a
run. The ghosting persists at every size tried, and the tester adds: **"it seems to get worse when
frame drops happen"**, and their own read is "some kind of eye submit desync, or the world geometry
isn't tracking the head tracking correctly".

**That read is now the leading hypothesis and it is testable.** The mod drives the game camera from
the head pose on the SCRIPT lane, and the compositor reprojects the submitted image using the pose
in the projection layer's views, located on the PRESENT lane. Those are two different samples of the
same head. If they disagree, the compositor's warp is wrong by the difference, and the error changes
every frame - which is doubled edges under rotation, and it grows when a frame is slow, which is
exactly "worse when frame drops happen". Nobody has ever measured that disagreement.

**The matched pair to measure it already exists in the code**: `head_track.cpp` records
`g_viewYawRad` (the yaw actually written to the engine) next to `g_injHmdYawSnap` (the HMD yaw it was
computed from) - its own comment says "Matched pair: this rotation was computed from THIS
g_hmdYaw". The instrument to build is: at submit, compare `g_injHmdYawSnap` for the frame that was
RENDERED against the yaw of the pose the layer is tagged with, and print the delta in degrees. It
can print the unwelcome answer - 0.0 deg means the two lanes agree and this hypothesis dies too.
`vrpace lag 0|1|2` and `vrpace ahead 0|1|2` are the live A/Bs that move it, and neither has been
judged.

### The tester's settings requests, with the REAL keys found (not guessed)

Asked for: killcam off, antialiasing FXAA, models high, maybe vsync on ("I just tried it and it was
maybe more smooth, but I'm not sure"). What the game's own config actually carries, in
`Documents\My Games\Dishonored\DishonoredGame\Config\`:

| ask | key | now | note |
|---|---|---|---|
| FXAA | `DishonoredEngine.ini [SystemSettings] iType_AntiAlias` | **1** (MLAA) | **2 = FXAA**, and the enum is documented in the file itself by the original devs (`EPpAa_None=0, EPpAa_Mlaa=1, EPpAa_Fxaa=2`) - measured, not guessed |
| vsync | same section, `UseVsync` | **False** | `True` is the ask; the tester is unsure it helped, so this one wants an A/B, not a default |
| models high | `SkeletalMeshLODBias`, `TextureForcedLODBias`, `DetailMode`, `Skeletal/StaticLODDistanceFactorMultiplier` | 0, 0, 2, 1, 1 | **already at the high end** (bias 0 = no reduction, DetailMode 2 = high). Nothing to change without inventing a value - do NOT write a guessed multiplier |
| killcam off | **NOT FOUND** | - | no killcam-shaped key in any of the 23 game inis (searched `Kill/Death/Assass/Slow/Cam` boolean keys). It may live in the save profile rather than an ini. Needs finding before it can be defaulted |

The AppCompat trap applies to all of these the same way it applies to the resolution: the bucket
AppCompat picks at startup overwrites `[SystemSettings]`, so anything written there must be written
to all four `AppCompatBucket*` sections too - `iType_AntiAlias` and `UseVsync` both already appear
in `DishonoredCompat.ini` with per-bucket values.

### Armed now: 3840x4096, purely to prove the lever to the eye

The tester asked to see the resolution do something visible, which is the right instinct and the
repo's own rule (confirm a lever moved something before believing its verdict). 3840x4096 keeps the
near-square eye aspect (0.9375 against 2064x2208's 0.9348) and is **3.4x the pixels** of the current
size, so if the lever were inert the tick would not move. Expect it to be slow - that IS the result.
Revert with `res 2064x2208f` (or `res 2496x2688f`) and relaunch.

## RESOLVED as a reading (2026-09-04): the submit stalls were NOT the game outrunning the headset

**The prediction written before the run held.** Submits landed at 88-115/s against 120 slots/s -
UNDER-SUBMITTING in 23 of 24 windows - and `endFrame mean` measured **0.08-1.99 ms**. xrEndFrame is
not blocking and is not throttling anything; the display slots are going unfilled. The reading that
opened this item ("the game produces frames faster than the headset can show them, and the runtime
absorbs the mismatch by blocking at submit") is **dead**, and it would still be alive if the period
had not been printed.

**What remains real:** 62 `perf: frame gap` lines, most still in `present-tail (xrEndFrame)` with
36-96 ms inside the submit call. Those are genuine, rare hitches, not pacing - and on a Wi-Fi
streaming runtime an 85 ms block in xrEndFrame is the encoder or the link, not our frame path. They
are worth a separate look AFTER the ghosting, because the ghosting is continuous and these are not.

**The tick, for whoever picks up "make it 120":** 9.2-9.9 ms, split almost evenly between the two
scene renders reentry needs - per pass roughly `present 1.8 ms + out/R 2.0 ms`, with our own capture
lock at 0.2-0.5 ms and endFrame at 0.0-0.6 ms. The mod is not the cost. Reaching 8.33 ms means
taking ~12 % off the game's own render, and the obvious dial is the 2496x2688 per-eye size.

## OPEN (2026-09-04): the submit stalls - 48 hitches of 44-156 ms, two thirds in xrEndFrame

**The report** (tester, 2026-09-04): "super laggy", and separately "this game is old enough that
it should run at hardlocked 120 fps, especially on a 4070 Ti Super". The headset is set to
**120 Hz**, so the budget is **8.33 ms** per frame.

**Throughput is NOT the problem, and that is the whole point of this item.** Measured on
`alpha-267-g4b9a0f3c-dirty`, Quest 3 via VDXR, 2496x2688 per eye, method reentry:

- **mean present interval 4.3-6.6 ms** (the heartbeat read GAME=138-233 fps across the run).
  That is comfortably inside the 8.33 ms budget, with 25-50 % headroom.
- **48 `perf: frame gap` lines**, gaps of **44-156 ms**. The worst is **36.3x the 4.3 ms mean** -
  at 120 Hz a 156 ms stall is 19 dropped frames in a row.

**Where the stalls sit**, from the `sat in:` field of those same 48 lines:

| phase | count |
|---|---|
| **`present-tail (xrEndFrame)`** | **31** |
| `out/idle (waiting for the game thread)` | 9 |
| `out/R (executing the frame)` | 3 |
| `game_tick` | 3 |
| the game's `Present` | 1 |
| `present-head (wait)` | 1 |

`flags:` on those lines read `reset=0 load=0 paceTimeouts=+0` almost throughout (one `+1`, one
`pairOpen=1`), so the pace lane is not timing out and the method is not re-arming. `vrpace ahead`
was at its shipped 0.

**Two thirds of the stalls are in the submit call.** So this is a pacing/submit problem, not a
rendering-cost one - which is the good news, because the fix is scheduling rather than cutting
quality.

### The gap that had to close first: the display period is now printed (session 13)

**It was not in the log.** `dvr::vr::display_period_ns()` existed in the runtime layer, was read by
the pace sync, and was PRINTED only inside the `PACE-BOUND` clause of the `perf: tick` line - so the
one run that most needed it (48 hitches, the wait at ~0, no `PACE-BOUND` line anywhere) is exactly
the run where it stayed invisible. "The game produces frames faster than the headset can show them,
and the runtime absorbs the mismatch by blocking at submit" was therefore an INFERENCE from a phase
NAME, and this project has spent whole sessions on inferences that read well and were wrong.

**What now prints** (built and lint-clean on `performance-fix`, **not yet run** - see below):

- **`stereo: rate`**, a new line on the 3 s stereo beat:
  `hmd=8.33 ms (120.0 Hz) slots/s=120.0 | presents/s=233 submits/s=116 (one xrEndFrame per pair) |
  endFrame mean=6.41 ms max=41.2 ms over 349 submits | <verdict>`.
  `submits/s` is a NEW counter: `xrEndFrame` calls from the present path, which under reentry is
  **one per PAIR** - it is the tick rate, not `out/s`. The endFrame mean and max are the submit's
  own cost, drained per window.
- **the verdict on that same line**, which is the whole point and can print the unwelcome answer:
  **OVER-SUBMITTING** (> 1.05x the slots) confirms the throttle reading and the lever becomes "stop
  producing frames nobody sees"; **MATCHED** (0.95-1.05x) means the endFrame MEAN is the pacing wait
  and is not a hitch, only its max is; **UNDER-SUBMITTING** (< 0.95x) says display slots are going
  unfilled, which falsifies the throttle reading outright and puts the cause upstream of the
  headset's cadence. A runtime that leaves `predictedDisplayPeriod` at 0 prints `hmd=UNKNOWN` and
  gets NO verdict - never read that as 0 Hz.
- **`perf: tick`** now opens with `[hmd 8.33 ms = 120.0 Hz, budget 8.33 ms/tick]` unconditionally,
  not only when pace-bound.
- **`perf: frame gap`** now ends its phase attribution with `= 19.0 display slots at 8.33 ms`. The
  "156 ms is 19 dropped frames" arithmetic in the table above was done by hand off the log; it is in
  the line now.
- **`status.json`**: `stereo.pair{displayPeriodMs, displayHz, endFrames, endFrameMeanMs,
  endFrameMaxMs}` and `perf{displayPeriodMs, displayHz}`. `game-cmd.ps1 "stereo status"` prints the
  same numbers live, without waiting for a beat.

**A prediction worth writing down before the run, because the arithmetic already argues against the
inference.** The report's own numbers are 4.3-6.6 ms mean PRESENT interval, and reentry submits one
frame per two presents - so submits/s should land at **76-116**, BELOW 120. If that holds, the line
reads UNDER-SUBMITTING and the "game outruns the headset" reading is dead: the headset would be
going hungry, not being over-fed, and the 31 present-tail stalls are genuine hitches inside
`xrEndFrame` rather than a throttle. The measurement is what settles it either way.

**Next step: a headset run on `performance-fix` with nothing else changed**, and the three lines
above out of the log. Nothing has been installed or launched for this change - the build is verified
only as compiling, linting clean and exporting the nine names.

### Not a fault, for the record

Stereo reads mono for roughly the first 6 seconds of a run (`2nd/s=0`), then latches to 103-108 and
holds - confirmed by the tester ("it is mono at the very beginning but it latches on and stays good
after a few seconds"). The `2nd/s=0` at the very END of a log is leaving gameplay, not a regression.

## FIXED (2026-09-03, headset, dev rig): all three flickers, in one chain

Branch `swapchain-one-picture-flicker`. Installed and judged as `alpha-253-g8441404f`. **The
tester's verdict after the third fix: every kind of flicker is gone.**

The three faults were nested - each fix exposed the next - and each was confirmed in the headset
and in the log before moving on.

### 1. The stale RIGHT eye (`1507bafc`)

`reentry.cpp`'s c5 invariant has two arms and they are **not equally trustworthy**. `inv=+1`
("pass 2 after pass 1") compares two draws with NO world tick between them: the step is exactly
`-ipd*scale` along the camera's right row by construction. `inv=-1` ("pass 1 after a still pass
2") is the **only arm that reasons across a world tick**, and holds solely while the player is
near still. A gently moving player - turning in place, decelerating, crouch-walk - parks the
tick's travel inside the `+-0.35*ipd` window (about `+-2.2 uu` at the measured 6.18) and the
fragile arm then names a genuine pass-2 present a pass 1. It did so **unconditionally, on a
streak of one**. One wrong `-1` writes the left swapchain twice and never writes the right.

The fragile arm now defers to the ring on a disagreement (a streak of three still earns the
override for either arm); neither arm invents a tag on an empty ring or over the `0` tag a
single gameplay draw pushes.

| | before | after |
|---|---|---|
| `STALE ? EYE` lines | 36 in 171 s | 1 in 238 s |
| one-picture pairs at sc | 14 of 39 (36 %) | 0-1 of 40 |
| `sc-target repeats` | 80 | 2 |
| `out/s` median | 117 | 212 |

`c5Held=36` counts the deferrals that did it, against `c5Agree=29210 c5Disagree=112`.

**Why no gate admitted to it**: `SceneDrawMaybeSecond` cannot skip pass 2 unlogged - `!doubleIt`
means pass 1 pushed no `-1` at all, the poison logs an `Error` at its one site and stands the
method down, and the forced skip IS the `forced=` field. Both tags were pushed every time; the
second `-1` was manufactured downstream. Every zero on the line was true.

### 2. The mono flick on the arms and weapon (`[Stereo] HoldUntagged`)

With the stale eye gone the tester saw a different flicker: "both arm models/weapons instead of
just the left handed crossbow", and it "felt slightly different". It was. `mono/s=1..4` in steady
gameplay, and a mono present is the same image in BOTH eyes, so its error scales with disparity -
near zero on distant geometry, **largest on the viewmodel at 30-50 cm**. Symmetric, hence both
arms, where the stale-eye fault was one-sided. 26 of 29 single-draw spells were the present-stall
guard; `c5Refused=72` was the rest.

`HoldUntagged=3` (ported from the parked branch) took `mono/s` 1-4 -> **0** and every other
flicker with it.

### 3. The black frame in both eyes (`8441404f`) - caused by the fix for 2

The hold immediately produced a new, subtler artifact: one black frame in both eyes, frequent.
**The whole layer assembly in `on_present_end` sits inside `if (backbuffer)`**, and
`backbuffer = frame`. A present that hands in no texture therefore reaches `xrEndFrame` with
`layerCount 0`, and a zero-layer frame gives the compositor nothing for that display slot -
black, both eyes, one frame. `HoldUntagged` made that path common: it holds a present by
returning false from the method, and `frame_hooks.cpp:181` passes the null to `on_present_end`
unconditionally.

The ported commit's own message claims "nothing is submitted; the compositor holds the previous
pair". **That is not true of this runtime**, and the commit was parked without a headset run that
would have caught it - it was taken as established behaviour instead of checked.

The guard now re-submits the layer the previous present used. Nothing is acquired or released on
such a present, so the swapchain images are untouched and the compositor genuinely re-shows the
previous pair; reprojecting it to the new display time is the runtime's job, and the
parked-session keepalive already re-submits that same snapshot. Measured over 126 s:
**`held=25 black=0`**, `STALE` 0, `sc-target repeats=0`, one-picture `sc=0`, `out/s` median 201.

### Levers and defaults

**`[Stereo] HoldUntagged=3` now SHIPS** (the user's call, 2026-09-03), joining the session-9
precedent of the headset-judged values being the defaults. It is a deliberate exception to the
default-OFF rule for render levers, made because the artifact it removes is a visible flicker on
the viewmodel and the black frame it used to expose is fixed at the runtime. `0` is the A/B and
the pre-41.1 behaviour; `stereo hold <n>` switches it live, `none/s` on the beat counts what it
held, and `zeroLayerHeld`/`zeroLayerBlack` in status.json say whether the guard covered them.

**Judged on ONE rig, for a few minutes.** A second headset that reports one-frame stalls or a
smeared weapon should try `stereo hold 1` and then `0`, and say which is better.

### The rig, and three traps that cost runs

Quest 3 via VDXR, 5120x1440@240 desktop, RTX 4070 Ti SUPER. `[Screen] RenderWidth/Height` in the
ini CANNOT affect the launch it is set on - DllMain reads only `dishonored_vr_launch.txt`, written
by the `res` seam word, so use `res 2496x2688` and never hand-edit the key. `bPauseOnLossOfFocus`
was TRUE in the game ini (now FALSE here): with it on, alt-tabbing to drive the command seam
pauses the thing being judged. VERIFICATION gotcha 17 bit 1 run in 3: state sticks at MENU in the
level, `2nd/s=0`, screen stays mono - open and close the pause menu.

**Check the build tag before reading any verdict out of a log.** The first "it's fixed!" here was
measured on a build that had been compiled but never installed - `build.ps1` had run, `install.ps1`
had not, so the game folder still held the previous DLL. The log banner names the build
(`build alpha-NNN-gHASH`) and `Get-FileHash` against `build\src\RelWithDebInfo\d3d9.dll` settles
it in one command. A `-dirty` tag means the tree had uncommitted changes at build time and the
log cannot be traced to a commit: rebuild from a clean tree before handing a log to anyone.

## Current state (2026-09-04, session 14: the throttle reading is dead, the cadence is the suspect)

**This branch (`performance-fix`) carries two instruments and one lever, all default OFF or
log-only.** No render path changed. Session 13 added the rate measurement, it was run, and it killed
the hypothesis the branch was opened for - the submit was never throttling. Session 14 added the
cadence measurement, which points at the ghosting instead, and `[Pace] SyncHz`, which ships at 0.

What shipped: the `stereo: rate` beat line (hmd period and Hz, slots/s, presents/s, submits/s, the
pair interval's mean and sd, the submit's own mean and max cost, and TWO verdicts - supply and
evenness - either of which can print the unwelcome answer); the display period unconditionally on
`perf: tick`; the gap converted to display slots on `perf: frame gap`; `[Pace] SyncHz` as the
persisted form of `vrpace sync <hz>`; and all of it in `status.json` and `stereo status`. The two
OPEN sections at the top carry the readings and what they mean.

**Built, lint-clean, exports OK, and INSTALLED as `alpha-260-g958ab57a`** (RelWithDebInfo,
hash-checked against `build\src\RelWithDebInfo\d3d9.dll`, clean tag - no `-dirty`). It replaces
`alpha-259-g865f1bcd`, so the game folder no longer carries the crouch fix: `crouch-fix` is still
only PR #14 and this branch is cut from `VR-Main`. **Not launched, not run** - in the simulator or a
headset. Treat every number it prints as unseen until a run produces one.

The two logs that were in the game folder are archived to `D:\dvr-data\logs\s13-pre-install-*.log`
before the run overwrites them (rotation is one deep and there were already two).

**The crouch height rise is FIXED and headset-judged** (previous session): the 38.16 deep-crouch
capsule write moved the pawn 20.00 uu on every crouch and the camera's rate-limited catch-up
integrated the remainder. `[PosTrack] DeepCrouch` now defaults to 0. That work is on `crouch-fix`
and is **open as PR #14, not merged**. Its derivation and the five falsified suspects are in
`docs/dishonored/ENGINE_NOTES.md` on that branch.

**The DLL in the game folder is `alpha-262-g61130855`** (session 14's build - the cadence instrument and
`[Pace] SyncHz`), replacing the session-13 `alpha-260-g958ab57a` that produced the run above. Both
are the tip of THIS branch, built
RelWithDebInfo, installed and hash-checked against `build\src\RelWithDebInfo\d3d9.dll`. It
replaced `alpha-259-g865f1bcd` (the tip of `crouch-fix`), so **the deployed build no longer carries
the crouch fix** - `performance-fix` is cut from `VR-Main` and PR #14 is still unmerged. Nothing
diagnostic is armed either way: `[Hands] CrouchAB`, `CrouchBurst` and `[PosTrack] DeepCrouch` are
all default OFF, and the tester's `dishonored_vr.ini` and the game's `DishonoredCamera.ini` were
restored from backups after the crouch investigation; no diagnostic keys remain in either.

**Two findings from that session are deliberately unbundled and unfixed**, because neither has been
judged in a headset:

1. `LocPropFind` and `CrouchPropFind` sit below `if (!g_handMesh) return` in `ApplyHandToMesh`, and
   `[Mode] GamepadOnly=1` (the shipped default) clears `g_handMesh` - so **they have never run on a
   shipped build**, and the 38.24 eye clamp, which needs `g_actorLocFound`, has never run either.
   Reviving it is a real behaviour change and needs a headset verdict of its own.
2. The config line reporting physical crouch as "armed" when `[Mode] GamepadOnly=1` has already
   vetoed it. Cost a session's hypothesis once already.

## FIXED (2026-09-04, headset, dev rig): the crouch height rise

**The report**: crouching then standing raises the player slightly, and spamming it rises far
enough to pass through the ceiling. Also, from the same run, "almost like noclip, I could float
around".

**The cause is ours**: the 38.16 deep-crouch write. Shrinking the crouched collision cylinder
(65 -> 45) under a grounded pawn moves the pawn's origin down by exactly the shrink, 20.00 uu,
because the engine keeps the feet planted - and sometimes leaves the pawn airborne. The camera
chases that with a rate-limited convergence that cannot finish before the next crouch, so the
remainder accumulates: ~20 uu of view per cycle, 2184 uu (22 m) in one run. Measured, filmed frame
by frame, and confirmed by an A-B-A A/B (+20.35 uu/cycle with the write on, -0.26 with it off).
Full derivation and the five falsified suspects in ENGINE_NOTES.

**The fix**: `[PosTrack] DeepCrouch` defaults to **0**. Judged in the headset by the tester: the
climb and the floating are both gone. The cost is that the player no longer fits under low
furniture; `DeepCrouch=1` restores the old behaviour and the bug with it. It cannot be made safe as
written without writing `Actor.Location`, which this mod deliberately never does.

## What this branch adds (session 10: the HUD panel and the cutscene policy)

**The question this session existed to answer is answered: YES, Dishonored's HUD draws separate
from the world cleanly on the native path**, and by a simpler rule than the DXVK fork needed. The
panel is built, verified on the simulator, and SHIPS ON ([Hud] Panel=1) pending a headset
verdict.

- **THE MEASUREMENT** (`core/gfx/draw_census`, `[Draws] Census=0`, `draws on|off|status|kill|
  unkill`, F10 Display): the game draws its world into an OFFSCREEN scene target and paints the
  whole HUD onto the BACKBUFFER at the tail of the frame - 14 draws per present of 1205, at
  ordinals 1177-1221, every one full-viewport, depth off, blended. The render target alone
  separates HUD from world with NO overlap. **Proven by picture, not by counter**: `draws kill`
  on the class removed the health and blood indicator and left the world pixel-identical; killing
  the whole population removed the HUD entirely and still left the world untouched.
- **THE FOURTH TERM, and how it was caught.** The first redirect used the three obvious terms and
  the panel came up holding the WHOLE FRAME. `dump hud` (new) writes the panel's own texture and
  showed the frame; `draws kill hud` with the panel on emptied the panel; so one of the fifteen
  draws was painting the world. It is the SCENE RESOLVE - a full-screen textured quad, opaque,
  depth off, full viewport, which satisfied every term the rule had. **Alpha blending** tells them
  apart: something drawn onto a finished frame must blend to sit over it, the frame itself is
  written opaquely. The rule is four terms and the redirect is 14 draws, not 15.
- **THE PANEL** (`core/gfx/hud_capture`, `[Hud] Panel=0 SlotScale=0.50`, `hud on|off|status|scale
  <f>`, F10 Display): the class is redirected into a private A8R8G8B8 target, copied at Present
  into a shared surface fenced both ways, alpha-repaired (`alpha = max(r,g,b)`, the original's
  additive look) into R8G8B8A8 and handed to the runtime layer's head-locked quad through
  `set_hud_texture_provider`, which had sat with zero callers since 41.0. It REFUSES out loud if
  `patterns.h` carries no measured fingerprint. Verified: the panel texture holds the indicator
  and the reticle and nothing else, the world is intact with no HUD in it, `reentry.xrs` 11/11 and
  the tick unchanged at 90/s.
- **THE GATE IS THE GAME STATE, not the draw** - the pause menu is drawn by the same class
  (measured), so a draw-only gate would sweep the menu onto the panel, which is the original's
  inherited bug (HANDOFF 8.4). The panel needs the runtime's own gate (a projection present
  carrying an eye tag) AND strict gameplay AND no power wheel.
- **THE CUTSCENE SCREEN NO LONGER FOLLOWS THE HEAD** (`[Cine] HeadLocked=0`, new default;
  reported from the headset the same day). A cutscene lands on the runtime layer's quad, and that
  quad takes `[Screen] HeadLocked=1` - which ships, because a gameplay screen belongs in front of
  your eyes. So the cutscene was glued to the face and swung with every turn. The game side now
  parks the flag for the length of the cutscene and restores it afterwards, which needed no edit
  to the runtime layer: its own comment says cinematic scenes were meant to keep the world-locked
  space, and our global flag was overriding that intent. Proven by picture on the simulator: at
  yaw 0 the quad reads 21.1 % of both eyes, at yaw 35 it reads 17.3 % / 24.5 % - it stayed in the
  room. A head-locked quad gives identical numbers at both yaws (session 5, run 9).
- **CUTSCENES HAVE A POLICY** (`[Cine] Mode=quad`, `[Cine] HudPanel=1`, `[Cine] HeadLocked=0`,
  `cine quad|stereo|hud|headlock|latch|status`, an **F10 Display block**, and the runtime's own
  `vrcine` seam is reachable at last). `quad`
  ships and is what has always happened; `stereo` holds the per-eye projection through the
  cutscene. Neither makes the matinee camera follow your head. `HudPanel` is the subtitle
  decision: on the panel (one image in both eyes) or in the frame (where text can double).
- **THE ARCHAEOLOGY IS WRITTEN DOWN** (ENGINE_NOTES, "The HUD and the cinematics: what the
  original did"): the fork's classifier term by term, the panel's placement math and blend, the
  clear rule's three eras, the dialogue window's four measured fixes, the settings with the
  tester's shipped values, the known bugs, and the runtime layer's cinematic subsystem with a
  LIVE/DEAD column per decision. The port was judged against it rather than guessed.

## Next steps (one paragraph per developer)

**The user (headset)**: nothing is blocking. All three flickers are fixed and measured (see FIXED
above) and this branch adds the HUD panel and the cutscene policy on top of them. The cutscene
screen following your head is FIXED (`[Cine] HeadLocked=0`), and the cinematic levers have an F10
Display block, which the first build lacked - that is why they could not be found. What only you
can judge, in order: (1) **The HUD panel** - is it legible at `SlotScale 0.50`, and is 1.25 m wide
at 1.30 m with a -0.10 m drop the right place? The HUD sliders on the F10 Runtime tab move it
live. (2) **Head-locked versus the old wrist**: the wrist anchor needs the hands back, so say
whether head-locked is good enough to keep. (3) **The panel's look**: the alpha repair is additive,
so dark HUD strokes go faint - does anything read wrong against a bright scene? (4) **Cutscenes**:
`cine stereo` gives depth but the picture holds its frame while you turn (the engine owns that
camera) - better or worse than the shipped `cine quad`? And the three carried from the flicker
work, which a correct flicker-free pair finally lets you judge: JUDDER on fast movement (`vrpace
ahead 0|1|2` on F10 Runtime), the PITCH PIVOT (`[Neck] Mode=cancel` against `off` and `add` on F10
Comfort), and WORLD SCALE and eye height at `[PosTrack] Scale=98` / `HeightOffsetM=-0.090`. One
open call that is yours, not a bug: whether `HoldUntagged` should stay at 3 - judged good on one
rig, and `stereo hold 0` is the A/B.

**The next developer session**: two leads from the headset run 47-04 first. (1) The new AV at
`Dishonored.exe+0xaf6a73` (reading address 1, thread "other", state NO_PAWN) appeared on the
one build that kept the projection through in-game menus; that build is reverted - if the EIP
recurs on the shipped build it is a separate lead, if it does not the menu change owned it.
(2) The tester reported intermittent stereo flicker on that build; the same run had the render
falling back to 2560x1440 fullscreen exclusive and the menu change live, both now fixed - judge
again before opening a ticket. Then: the wrist anchor is the natural follow-on and it needs the hands
(`[Mode] GamepadOnly=0`, `hands`/SkelControl on the winning method); `hud_panel.cpp`'s billboard
math is quoted in ENGINE_NOTES for it. Then: the letterbox bars during a cutscene were never
measured (no cutscene was reachable on the simulator lane this session) - run `draws on` through
one and see whether the bars are in the backbuffer class, because if they are they will land on
the panel; `[Hud] SkipPs` is the intended answer and is not built yet. Carried from session 9: the
ini version rewrite still wipes a tuned ini, `stereo aer` is still a design stub, the SteamVR shim
has never run with this game. New and small: `DumpTexturePng` swaps red and blue for R8G8B8A8
textures, so `dump eyes` and `dump hud` are wrong on COLOUR (never on geometry) - the compositor
captures are correct, and nothing has ever depended on it, but it should be fixed before someone
reads a colour verdict off a dump.

**The next developer session (the ghosting, from VR-Main)**: build the POSE-LANE instrument and stop guessing.
Three suspects have now been killed by measurement - the submit throttle, the uneven cadence, and
Virtual Desktop's SSW - and the one the tester named has never been measured at all: the camera is
driven from a head sample on the SCRIPT lane while the compositor reprojects using a pose located on
the PRESENT lane, and nothing anywhere compares the two. `head_track.cpp` already keeps the matched
pair (`g_viewYawRad` beside the `g_injHmdYawSnap` it was computed from, its own comment says so);
publish that snap to the runtime and, at submit, print the delta in degrees against the yaw of the
pose the layer is tagged with. It must be able to print 0.0 and kill the hypothesis. Then
`vrpace lag 0|1|2` and `vrpace ahead 0|1|2` are the live A/Bs that move it, and neither has ever
been judged. The corroborating detail worth keeping in mind: the tester says the ghosting **grows
when frames drop**, which is what a lane-disagreement does and what a locked cadence does not.
Second job, small and separable: the game-settings profile (the real keys and what is already at
maximum are tabulated in the OPEN section - `iType_AntiAlias` 1 -> 2 is the only clear win, `UseVsync`
wants an A/B, model detail is already high, and no killcam key exists in any of the 23 game inis).
Anything written to `[SystemSettings]` must also go to all four `AppCompatBucket*`, or the bucket
AppCompat picks at startup overwrites it - the same trap the resolution picker already handles.

**The user (headset)**: installed as `alpha-264-ge2b84a80`, with **3840x4096 armed for the next
launch** (your call - you judged 4K much better looking). Know the trade: it measures 15.6-15.8 ms
per tick = 64 fps into a 120 Hz headset, against 8.6-8.9 ms at 2064x2208, so if the ghosting really
does track frame drops, 4K is the worst case for it and the sharpest picture at the same time. If
you want the middle, `res 2496x2688f` or `res 2064x2208f` in-game then relaunch. Nothing else is
waiting on you until the pose instrument exists - the three cheap A/Bs are spent. `crouch-fix`
(PR #14) is still ready for your merge decision, and note that the installed build does NOT carry it.
Still open from earlier sessions: (1) the PITCH PIVOT with `[Neck] Mode=cancel` against `off` and
`add`; (2) WORLD SCALE and eye height at `[PosTrack] Scale=98` / `HeightOffsetM=-0.090`.

## Blockers

- **Nothing blocks the code.** The panel and the cutscene policy both ship off and both are
  verified on the simulator; what is left is perceptual and needs the headset.
- **The ini version rewrite wipes a tuned ini** (carried, session 9): the session-8, -9 and -10
  keys ship without a version bump. A key-preserving rewrite is a separate change.
- **WM_CLOSE leaves a stuck `Dishonored.exe`** (session 5): close a healthy game with
  `Stop-Process`; a menu quit is clean on the Quest.
- **The walk-in on the simulator is by hand**: Return x4 with 25 s waits; look at an
  `xrsim-shot` before trusting a state line (VERIFICATION gotcha 17).

## Session log

### 2026-09-04 - session 15b: the ghosting is solved, and the verdict that hid it is fixed

Two headset runs, same build, same scene, same 2750x2850 render, only the headset's refresh
changed. 120 Hz: ghosting still reported. 90 Hz: **none reported, and no jitter**. Display
slots per frame went 1.05-1.11 -> 1.00-1.02.

**The cadence hypothesis was right all along, and session 14 killed it on a lying verdict.**
The `EVEN CADENCE` threshold was `|off| > 0.06`, so 1.03-1.05 - a beat every twenty frames -
printed as a clean bill of health, and that clean bill was read as falsification. The
instrument was correctly built and correctly read; the line between pass and fail had simply
been picked before anything was measured. Threshold is now 0.02, at the measured edge, and both
branches print the beat as a number (one frame in N, and its Hz) so an "even" verdict has to
show the residual it is forgiving.

**The tester's puzzle - why it drops to 60 at 90 Hz when it never went below 90 at 120 Hz -
has an answer, and the hitch rate is the proof.** Normalised by run length: 27.6 gaps/min at
120 Hz, 28.4 at 90 Hz. The stalls did not get worse. At 120 Hz the tick never fit the slot, so
the app free-ran and the compositor smeared over the mismatch - no cliff to fall off when you
are already past the edge, and that smearing IS the ghosting. At 90 Hz the tick sits right at
the period: frames make their slots (ghosting gone) but a miss costs a whole period, which is
22.2 ms, which averages toward 60 in a run. Smooth-and-always-wrong versus correct-with-a-cliff.

**The remaining drops are not ours.** 54 of 71 gaps sat in `present-tail (xrEndFrame)`, up to
101 ms. On a Wi-Fi streaming runtime that is the encoder or the link.

**The bbox prediction failed and is recorded as failed.** Gating the 3-second readback cut
samples from one per 3 s to 2-3 per run and changed the gap rate not at all. The
counter-evidence written down beside the prediction was the correct read. The gate stays - it
removed a real unlevered stall for free - but it did not fix what it was predicted to fix.

**The pose-lane instrument validated itself and was never armed.** `SEAM CHECK ok ... 20.67 deg
and 20.67 deg (0.00 apart)` in both runs: the sign calibration is proven against live data, so
the instrument would not have lied. Nobody ran `vrpace poseaudit on`, so the pose-attribution
question stays open - it is just no longer the ghosting's suspect.

Defaults now carry the judged values (`[Screen] 2750x2850`, with the 90 Hz half written into
the ini text beside it because it lives in Virtual Desktop), and
`tests/golden/known-good-2750x2850-90hz.ini` is the byte copy of the machine that was judged.

### 2026-09-04 - session 15: the pose lanes get an instrument, and a 3-second stall is found

Session 14 falsified the cadence hypothesis and left three suspects. Two of them died at the
desk, from files already on disk, before anything was written:

- **Virtual Desktop's SSW** - the tester confirmed it was already off.
- **Motion blur** - `MotionBlur=False` in `DishonoredEngine.ini`, with the Arkane developers'
  own comment beside it: "Motion blur is unwanted".

That left the tester's own read - "eye submit desync, or the world geometry isn't tracking the
head tracking correctly" - and it turned out the instrument for it was **half-built and
unreachable**. The pose audit already sat at the right line (immediately after the projection
views are filled, before they are attached), already wrapped to +-180, already rate-limited at
500 ms. Two things were wrong: it compared the tag against the pose the present thread had just
CONSUMED, which is fresh at submit and therefore never the sample the pixels came from - so it
could not answer the question it was named for - and **`set_pose_audit` had no caller at all**.
The `fovaudit pose on` command its comment named does not exist in this repo. It was dead code
that would have printed a confidently wrong number if anyone had reached it.

**What was built.** A locate generation counter bumped once per `xrLocateViews`; the game side
stamps every head sample with the generation it came from and publishes it, with the yaw the
camera write actually used, through a new `dvr::vr::publish_script_head` seam (needed because
`g_injHmdYawSnap` is static inside the unity TU and the runtime layer is a real module). The
audit now reports, per eye, the tagged yaw against the rendered yaw, the gap in GENERATIONS
against the active `lag`, and what one generation costs in degrees at the current head speed.
`vrpace poseaudit on|off` arms it.

**The sign trap, and why it is self-checking.** The two lanes read yaw out of the same matrix
with opposite conventions - `atan2(m02,m22)` against `atan2(-m02,m22)` - so a naive subtraction
reads about twice the yaw. That is the most convincing possible way for an instrument to lie:
a large, stable, entirely fake disagreement. The seam negates once and then PROVES it against
live data, reading the same pose back through the runtime's own converter at the first publish
and logging `SEAM CHECK ok` or `FAILED`. An instrument whose calibration is only asserted in a
comment is not evidence.

**The prediction, written before the run.** `DishonoredEngine.ini` carries
`OneFrameThreadLag=True`. The tagging code assumes one generation of skew (`lag=1`); with UE3's
render thread a frame behind the game thread it should be two. So: gap +1 at `lag 1`, nulled by
`vrpace lag 2`. If the delta reads 0.00 at `lag 1`, the hypothesis is dead and that is the
result.

**Found on the way: a full-frame CPU readback every 3 seconds, on the present thread, in the
shipping capture mode, with no lever.** The content-bbox instrument needs CPU pixels, and
`shared` mode exists precisely so that nothing goes to the CPU. Each sample is the same
`GetRenderTargetData` + `LockRect` + row copy that makes `sync` mode cost 17-21 ms/present -
about 31 MB at 2750x2850. `[Capture] BboxMs` (default 30000) and `capture bbox off|<ms>` gate
it; a size change still resamples at once. The prediction and the counter-evidence are both
recorded at the top of this file.

**Performance, answered with arithmetic instead of another run.** Fitting the three tick
measurements against megapixels gives ~0.64 ms/MP on a **~5.6 ms fixed floor**. The floor is
what makes 120 Hz unreachable - it leaves 2.7 ms of GPU for two full-frame scene draws - so the
resolution question is really a refresh-rate question. 2750x2850 at 90 Hz is the coherent
combination and is what is armed. There is no render-scale, no foveation and no per-eye
resolution anywhere in the codebase; resolution is the only pixel lever that exists.

**New tool:** `tools\arm-res.ps1` arms a size with the game not running, writing the same four
places `ResRequest` does. Arming used to cost two launches (the seam command only exists while
the game is up, and its write takes effect the launch after).

**Nothing was launched.** Everything here is built, linted, exports-checked and installed, with
the format strings audited by hand; no runtime behaviour is verified.

### 2026-09-04 - session 14: the throttle reading dies, the cadence is named

The session-13 instrument was installed and run (`alpha-260-g958ab57a`, Quest 3 / VDXR, 120 Hz) and
it did its job in both directions.

**It killed the hypothesis it was built to test.** submits/s 88-115 against 120 slots/s -
UNDER-SUBMITTING in 23 of 24 windows - with `endFrame mean` at 0.08-1.99 ms. The submit is not
blocking and never was; the display slots were going unfilled. Written prediction, held.

**It pointed at the real one.** The tester's bigger complaint on that run was ghosting - doubled
edges on world geometry when turning the head. `perf: tick` reads 9.2-9.9 ms against an 8.33 ms
slot, and `pacetrace.log`'s `TRACE pairs` reads interval mean 8.6-11.8 ms with **sd 1.3-9.6 ms** and
`waitGate 3-64 ms/s` - the game free-runs at ~1.13 display slots per frame, unevenly, so consecutive
frames are held for different numbers of slots. That is the interference beat `pace_sync_gate()` was
written for in the BioShock lineage, and its own comment predicted this shape of fault.

Shipped: the pair interval mean/sd and an `UNEVEN CADENCE` / `EVEN CADENCE` verdict with
slots-per-frame on the `stereo: rate` line (the numbers existed only in `pacetrace.log` at trace
level, which is why no one had seen them), and `[Pace] SyncHz` - the persisted form of
`vrpace sync <hz>`, shipping OFF, refusing an out-of-range value with the number it read.

**The proposed fix was wrong and was corrected in the same session.** `vrpace sync 60` locks the
cadence but 60 is too low for VR, and the user said so. Infinite's own numbers on the same runtime
say why no limiter is needed: it ran 80 pairs/s == its 80 Hz refresh with sd 0.3-1.0 ms, LOCKED, on
the same two-draw method - because its render cost fit inside its period. Ours does not (9.4 ms into
8.33), and we are also at 47 % more pixels per eye than Infinite's native. So the lever is the gap
between tick and period, from either end: `res 2064x2208f` (the Quest 3 panel's own size) or a 90 Hz
headset. Still a prediction; nobody has judged either in a headset.

Installed as `alpha-262-g61130855` (clean tag); builds, lint clean, exports OK.

### 2026-09-04 - session 13: the display period is measured, not inferred

The branch's first code. **One commit, instrument only** - no lever, no default, no render path.
`dvr::vr::display_period_ns()` had existed since session 42 of the BioShock lineage and printed in
exactly one place, inside the `PACE-BOUND` clause of `perf: tick`, which is a clause the hitching
run never triggered. So the headset's rate was absent from the one log that needed it.

Added: an `xrEndFrame` counter with its own cost (count, cumulative sum, per-window max) at the
single present-path submit site, and the display period, both published through `PairProbe`; the
`stereo: rate` beat line built on them, with a three-way verdict (OVER-SUBMITTING / MATCHED /
UNDER-SUBMITTING, plus UNKNOWN when the runtime leaves the period at 0); the period unconditionally
on `perf: tick`; the gap in display slots on `perf: frame gap`; the same numbers in `status.json`
and in `stereo status`.

**A prediction is on the record before the run** (OPEN section): the report's 4.3-6.6 ms present
interval, halved by reentry's one-submit-per-pair, puts submits at 76-116/s against 120 slots/s -
UNDER-SUBMITTING, which would falsify the throttle reading outright. The line was written so it can
say that.

**Verified as: builds, `lint: clean`, `exports OK: 9 names`, installed and hash-checked
(`alpha-260-g958ab57a`).** Not launched, not run in the simulator or a headset - every number the
new lines print is still unseen.

What was already measured before this session and should not be re-derived:

- The rig has **headroom**: mean present interval 4.3-6.6 ms against an 8.33 ms budget at 120 Hz.
  The complaint is not framerate.
- **48 hitches of 44-156 ms**, and **31 of 48 sat in `present-tail (xrEndFrame)`** - the submit
  call. `paceTimeouts` and `reset` were 0 throughout, so the pace lane is not timing out.
- **The display period is not logged**, so the obvious reading (the game outruns the headset and
  blocks at submit) is an inference. Measuring it is step one.
- Mono for the first ~6 s of a run then latching to 103-108 `2nd/s` is NORMAL and tester-confirmed;
  do not chase it.

### 2026-09-05 - session 11: the work is on a board, and the flow is written down

Branch `claude/linear-github-integration-9d1a52`, docs and templates only. No code, no build,
no render lever touched.

Three people are now finding faults and recording them in three private places, and PRs #12,
#14 and #15 each name real, measured, open defects that appear in no shared list. The
Linear workspace `vr-stereo-hub` existed but was empty.

**The board now matches the code.** Team `VR`, project **Dishonored VR Mod** with a lead, a
spec and its doc links; four release-shaped milestones (41.1, 41.2, 42.0, 42.1) kept in sync
with the GitHub Releases page, each naming the ROADMAP rungs it closes; a `Type` label group,
ten `area:` labels and five flag labels; **43 tickets** (VR-6 to VR-48) covering every open PR,
every unticked ROADMAP box, every genuinely open KNOWN_ISSUES entry, the three STATUS blockers,
and the four defects PRs #14 and #15 handed over. Merged work is recorded as **one catch-up
project update**, not as retroactive tickets. The four open PRs carry `Fixes VR-<n>` and two
were retitled to conventional-commit subjects.

**The flow is `docs/LINEAR_AND_GITHUB.md`**: statuses and what each means here, priority,
labels, the ticket template, the numbered ticket-to-release flow, the PR contract, project
updates, the release ritual, and what only the Linear UI can do. `CLAUDE.md` carries the hard
rules and the session protocol now names the ticket step. `.github/` gains a PR template, two
issue forms and `config.yml`; `CONTRIBUTING.md` is the front door.

**Two rules adopted.** From PR #15: **never quote a chat verbatim** in anything published -
commit messages, PR bodies, tickets, comments, `docs/`. The observation is evidence; the
wording never is. And: **an agent never declares a release.** It may report that a milestone is
clear and ask.

**Not done, and it needs the user.** The `Released` status does not exist yet and the PR
automation rows are unset: the Linear MCP exposes no tool for either, and both are team
settings. More importantly **the magic-word link did not fire** - Linear received the PR edits
(the diff records updated) and created no link, so `linkedIssues` is still empty on all four.
The ids and magic words are correct, so this is the GitHub integration's issue-linking side
not being enabled for the repo. Until it is, the PR-to-ticket links are the plain attachments
created by hand and no status automation will work.

### 2026-09-04 - session 12: the crouch height rise, solved

- **The answer**: the 38.16 deep-crouch capsule write moves the pawn 20.00 uu on every crouch (the
  engine keeps the feet planted, so a shorter capsule means a lower origin), and the camera's
  rate-limited catch-up never converges before the next crouch. `[PosTrack] DeepCrouch` now
  defaults to 0. Headset-judged: fixed.
- **The method lesson**: naming a mechanism and flipping its lever failed FIVE times running
  (physical crouch, the engine's uncrouch arithmetic, our eye clamp, the game's bump smoother, our
  own crouch eye-drop). What worked was filming the pawn and the camera one line per frame across
  the transition and letting the shape of the curve name the moment. When levers keep coming back
  null, stop naming suspects.
- **Two A/B traps paid for**: an interleaved A/B measures a system with memory BACKWARDS (the eased
  eye clamp redistributed the effect across the cycle boundary and the per-cycle median reported
  the sawtooth, not the climb - blocks with settling cycles fixed it); and a lever that was never
  connected reads as a clean FALSIFIED (the eye clamp had never executed at all). Confirm a lever
  moved something before believing its verdict.
- **A symptom mentioned in passing was the mechanism speaking**: "I could float around" turned out
  to be the pawn genuinely airborne, filmed rising 34 uu and falling 128 uu back to the floor.

### 2026-09-04 - session 13: the stale RIGHT eye, read out of the code, then confirmed

### 2026-09-04 - session 10: the HUD and the cinematics, ported onto the native render

Branch `claude/dishonored-vr-hud-cinematics-149a8e` -> PR into `VR-Main`. Runs on the dev PC
(simulator lane, the sewers, `stereo reentry`, 2496x2688, shipped defaults; logs in
`D:\dvr-data\logs\46-run*.log`):

| Run | What | Result |
|---|---|---|
| 01 | the census, first build | 1205 draws/present (DIP 382, UP 13, IUP 810), 133 buckets, 81 shaders, 1 state block; `tonemap draws/present=0.0` and the VERDICT read NO HUD-CLASS DRAWS - the "after the tonemap" term rejected the whole frame, and the near-miss list named it. Two more of the fork's terms fell here: IUP carries 810 WORLD draws, and requiring a texture keeps 1 HUD draw of 15 |
| 02 | the corrected rule + the backbuffer table | **5 buckets, 15.0 draws/present, ordinals 1177-1221, all full-viewport and depth-off**; the pause menu: 10 buckets, 95.9/present, same shape. `draws kill` by key: the indicator gone, the world pixel-identical; all five killed: the HUD gone, the world untouched. `separators with NO overlap: rt` |
| 03 | the panel, first build | the quad reaches the compositor (`xr: HUD quad live 1248x1344`) but holds the whole frame; read fences spun every present (`31767 waits, 1033 timeouts` - a D3D11 event query needs a Flush, and a 10 ms budget cannot be measured with GetTickCount) |
| 04 | `dump hud` + the unconditional clear | the panel texture IS the frame; with `draws kill hud` armed the panel is EMPTY - so a redirected draw was painting the world. It is the scene resolve, the one opaque draw in the population |
| 05 | the blend term | 14.0 draws/present redirected; the panel texture holds the indicator and the reticle and nothing else; the world intact with no HUD in it; fences 0 timeouts; `reentry.xrs` 11/11; `perf: tick 11.1 ms (90/s)`, pace-bound, unchanged |
| 47-04 | **HEADSET (the user)**, build 201 with in-game menus on the panel | navigating the pause menu broke VR; a NEW AV in game code (`Dishonored.exe+0xaf6a73`, state NO_PAWN, never logged before); `res: NOT HONOURED` - the mod ini had been hand-edited to 2750x2850 while the launch file and both game inis still said 2496x2688, so the game fell back to 2560x1440 FULLSCREEN, which is what read as "the resolution reset". Menus-on-panel REVERTED; the size re-armed in all four places with `arm-res.ps1 2496x2688`; verified `res: HONOURED`, panel armed, pause menu on the screen as before, 0 exceptions, `reentry.xrs` 11/11 (run 47-05) |
| 06 | the cutscene policy | `hud-panel.xrs` 20/20 and `cine-latch.xrs` 24/24: quad mode drops to one head-locked quad (`projectionViews 0`), stereo mode holds two projection views through the same latch |

This ran in parallel with the flicker branch below and merged after it; the two touch different files.

### 2026-09-04 - session 10: the stale RIGHT eye, read out of the code, then confirmed

Branch `swapchain-one-picture-flicker`, 6 commits, one headset run at the end.

The hand-off pointed at `SceneDrawMaybeSecond`'s unlogged early returns. They are a dead end,
and ruling them out is what found the fault: `!doubleIt` means pass 1 pushed no `-1` at all,
the poison logs an `Error` at its one site and stands the method down, and the forced skip IS
the `forced=` field. So the game side pushed both tags on all 20 stale submits, every zero on
the line was true, and the second `-1` had to be manufactured downstream of it.

It was, in `reentry.cpp`'s c5 pairing block, whose two arms are not equally trustworthy. Full
reasoning in the FIXED section above and two entries in ARCHITECTURE's decision log. The
asymmetry was the tell: only the `-1` arm can misfire, and only a wrong `-1` strands the right
eye. `L=0 R=20` is that, arithmetically. **Confirmed in the headset**: 36 stale lines in 171 s
became 1 in 238 s, sc one-picture 36 % became 0-1 of 40, `sc-target repeats` 80 became 2, and
`c5Held=36` counts the deferrals that did it.

The second finding is about the instrument, and it is the more expensive one. The STALE line
carried the game side's gates and the runtime's failures but **not one counter from the method
between them**, and its owner string mapped `abortLeft` straight to "the game side skipped pass
2" - a cause it never measured. Nine logged instances, three readers, all sent to the wrong
file. Worse, the block's unconditional `tagged = true` made `method untagged presents` read 0
*because* a tag had been invented: the counter did not merely miss the fault, it denied it.

**And a process trap that cost a run**: the first "it's fixed" verdict came from a run of a
build that did not contain the fix. `build.ps1` had run, `install.ps1` had not. The log banner
and `Get-FileHash` against `build\src\RelWithDebInfo\d3d9.dll` catch it in one command, and
that check now leads the rig notes.

| Change | What |
|---|---|
| `1507bafc` | the cross-tick arm defers to the ring (streak 3 still overrides); no invented tags on an empty or 0 ring; `sameEyePushed` counted at the push site; the c5 counters and a corrected owner string on the STALE line |
| `12c23588` | ported: `[Stereo] HoldUntagged`, default 0, `stereo hold <n>` - now the lever for the residual mono flick |
| `539b9391` | ported: the `pair geom` separation-angle line, every 2 s |
| `9620d437` | ported: F2 stamps the fault marker, eyes-free |
| `00b833a8` | ported: the `res` seam writes its ini path with a real separator |

**All three fixed, each confirmed in the headset before moving on.** The chain: the c5 fix
exposed a mono flick on the arms (disparity is largest on the viewmodel, so a mono present shows
there and nowhere else); `HoldUntagged=3` removed that and exposed a black frame in both eyes;
the black frame was a zero-layer `xrEndFrame`, because the layer assembly sits inside
`if (backbuffer)` and a held present hands in no texture. Final run: `held=25 black=0`, `STALE`
0, `sc-target repeats=0`, one-picture `sc=0`, `out/s` median 201.

| Change | What |
|---|---|
| `1507bafc` | the cross-tick arm defers to the ring; no invented tags; `sameEyePushed` and the c5 counters on the STALE line, and a corrected owner string |
| `8441404f` | a zero-layer xrEndFrame re-submits the previous layer; `zeroLayerHeld`/`zeroLayerBlack` counted and logged |
| `12c23588` | ported: `[Stereo] HoldUntagged`, default 0 - the lever for the mono flick |
| `539b9391` `9620d437` `00b833a8` | ported: `pair geom`, the F2 fault marker, the `res` seam separator fix |

**Two process lessons, both paid for this session.** A parked commit's message is not evidence:
`HoldUntagged`'s claim that "the compositor holds the previous pair" was false against this
runtime and had never been run in a headset, and taking it at face value is what put the black
flicker in front of the tester. And check the build tag before reading a verdict out of a log -
the first "it's fixed" here was measured on a build that was compiled but never installed.

### 2026-09-04 - session 9: the eyes - the trace, the swap, the fix, the proof

Branch `claude/dishonored-vr-both-eyes-same-659cb5` -> PR #7, 13 commits. Runs on the dev PC
(simulator lane, RTX 4060, 2496x2688 VirtualMode, the sewers, shipped defaults; logs in
`D:\dvr-data\logs\45-run*.log`) and the user's Quest 3 through VirtualDesktopXR:

| Run | What | Result |
|---|---|---|
| 01 | the trace and the words, first build | `stereo: frameid` pairs from the arming: L-R 4.1 at bb/slot/out, floor 1.5, c5 6.17, busy 0; `reentry rearm 2` -> SINGLE x2 then DOUBLE; `capture reinit` -> REBUILT, no STALE; `dump eyes` queued + written off-thread, no gap, no LOADING; the sc stage empty (an ordering bug) |
| 02 | the sc stage, the side check | sc reads (4.7-5.0); **the side flipped across `reentry rearm 2`** and within a second of the first arming; the ring overflowed 363 times in the menu |
| 03 | the 0-tag push + the c5 pairing (first form), the A/B | side ok from the first pair; `reentry c5pair off`: the side flipped on its own twice in 25 s, `untagged 16-19` per window; on: no flips |
| 04 | the invariant as the pairing, the picture shift | side ok + shift -1 px on every pair, P1 == P2, untagged 0-1; `reentry.xrs` 11/11 |
| 05 | the drain to the next expected tag | side ok from the first pair across a `stereo mono` -> `reentry` switch and a rearm; 0 ring drops |
| 06 | the F10 EYES block | the overlay renders the readout and the buttons in the headset's own view (`xrsim-shot`) |
| 07 | **HEADSET (the user)**, the session's build | the eyes RIGHT from the load and after every button; `side ok` / `SWAPPED=0` on every pair, c5 6.11, shift negative, L-R 3-14 (one picture = 1.5); the ring skewed 131 times in ~4 min - the old swaps, absorbed; the trace read every present cost 1.5 ms GPU idle per present, tick 16.7 ms (60/s under 72 Hz) |
| 08 | **HEADSET (the user)**, the sampled trace + the A/B | "the fps is perfect now"; `c5 pairing` OFF + pause/resume -> the fault returns (`swapped=24 of 25`, then 12 of 12, the picture agreeing), ON -> `swapped=0` for the rest of the run. **The root cause is proven.** |

### 2026-09-03 - session 8: performance - the tick budget, the census, the 9Ex device, the shared capture

Branch `claude/dishonored-vr-perf-9f4b10`, 20 commits. Runs on the dev PC (simulator lane, RTX 4060,
2496x2688 VirtualMode, logs in `D:\dvr-data\logs\44-run*.log`):

| Run | What | Result |
|---|---|---|
| 01 | the tick budget, sync / deferred / off | stereo sync: tick 46 ms (21/s), capture 17-21 ms per present of which lock 9-13, GPU dma 15.5-16.8 vs 3D 4.8; deferred: 36 ms (27/s), lock 0, dma 10.4; off: 93 presents/s pace-bound; the marker 1 BeginScene per present, 0 late GPU reads; `mark` and the gap line print; `reentry.xrs` 11/11 |
| 02 | the creation census | 8060 of 8120 creations MANAGED (398 MB), READONLY texture locks 10598, no AUTOGENMIPMAP; the shadow route decided |
| 03 | `[Device] Ex=1 Managed=shadow` | `CreateDeviceEx -> 0x0`, IS 9Ex, `shared surface AVAILABLE`; 5240 twins, 65552 updates, 0 failures; the sewers intact; shared (one slot) 0.2 ms per present |
| 04 | the fenced two-slot shared capture, stereo | SharedWait=1: tick 13.3 ms (75/s), lock = the 3.6 ms fence wait, dma 0.2; SharedWait=0: 11.1 ms (90/s) PACE-BOUND; `reentry.xrs` 11/11; hammer 0 stale over 5 cycles; the frame intact |
| 05 | the final build | deferred default 27.7/s; `capture mode off` with 0 STALE lines (the no-frame fix); `focus lose 2500` / `focus regain`: `eaten=0`, 0 stale; `reentry.xrs` 11/11 |
| 14a | **HEADSET (the user)**, Ex=0, deferred | 30-33 ticks/s at 2496x2688 (dma 10.9 ms per present), 50 gap lines, no attack freeze felt |
| 14b | **HEADSET (the user)**, Ex=1 | 9Ex device up, shared AVAILABLE but the capture stayed deferred (30 ticks/s); after repeated quickloads the twin map filled with tombstones and the game crashed in D3D9 (minidump `dvr_20260903_212436.dmp`) |
| 06 | the tombstone fix, Ex=1 + shared, 3 quickloads | 2324 live, 1984 tombstones reused of 32768, 0 failures, the game alive; 90 ticks/s pace-bound |
| 15 | **HEADSET (the user)**, Ex=1 + shared | "performance is pretty good"; the eyes disagree "90 % of the time, more at the beginning": 0 STALE, 0 tag mismatches, pairs one IPD apart, 32 pause/resumes each with a 1-1.5 s flat spell |
| 07 | the read fence + the menu resume, shipped defaults | `readWaits` 14 in the run (the race was real); hammer 10 cycles 0 stale, `view live at once` x11; `reentry.xrs` 11/11 |

### 2026-09-03 - session 7: the four headset faults and the picker, on the simulator

Branch `claude/dishonored-vr-stereo-polish-449d43`, 15 commits. Runs (simulator lane, logs in
`D:\dvr-data\logs\43-run*.log`; the run-40 headset log archived as `42-run40-quest3-verdict.log`):

| Run | What | Result |
|---|---|---|
| 01 | commits 0-1d | `Method=mono applied after the game side registered`; the gate decision logs its reason; `reentry.xrs` 11/11, `stale-eye.xrs` 18/18; hammer 10 cycles PASS, ages L=1 R=0 |
| 02 | `reentry skip2 120` | strict off: sim stale 0 -> 2, mono +62, `STALE R EYE` (owner first "unknown", then the game side); strict on: stale unchanged, 37 fallbacks to mono |
| 03 | the phase + ahead | runtime clock extension on the sim; phase +58 ms mean (synthetic); `vrpace ahead 1` logs and locates; hammer 5 PASS |
| 04 | pitchtest x3 | engine neck 0.321/0.062 m (cons 0.3 uu); the arc reached c5 on top of it; `neck cancel` -> travel < 0.5 uu; the picture agrees |
| 05 | Armed + console | park/re-arm on the seam correct; the first console word overflowed the game thread's stack (the hook re-entered) |
| 06 | the guard, boot | `Method=reentry Armed=1 -> active reentry` before the first present; `setres 2560x1440f`/`1600x900w` dispatch, empty reply, no Reset: INERT |
| 07-08 | the ini route | 2560x1440 fullscreen in every ini place -> `CreateDevice 1920x1080 windowed=1`: the ini is inert |
| 09 | the command line | `-ResX=2560 -ResY=1440 -FullScreen` via 3 import slots -> `CreateDevice 2560x1440 windowed=0`, capture and swapchains followed |
| 10-11 | 2496x2688, no VirtualMode | the game asked the mode list, fell back to 2560x1440 (a harness launch had restored the mod ini; the launch file carries the token now) |
| 12 | **2496x2688 with VirtualMode** | our mode handed at slot 123; `CreateDevice 2496x2688 windowed=1`; `res: HONOURED`; hfov 108 deg; both eyes 77 % non-black in the sewers; the frame complete; readback 18-20 ms/present |
| 13a | **HEADSET (the user)**: 2560x1440 fullscreen asked | Reset 2560x1440 twice then 2508x1411 windowed=1 (the game's own fallback); the run sat in menus, stereo never armed (`state` skips); readback 5.3-5.8 ms/present |
| 13b | **HEADSET (the user)**: 2496x2688 VirtualMode | `CreateDevice 2496x2688 windowed=1`, HONOURED, "pretty sharp"; `neck cancel` right; stereo L/s=R/s=16-28, ticks 28/s, readback 13-15 ms/present; one `STALE L EYE` (age 567) at a FOCUSED regain; the desync still seen on load; judder unjudgeable |

### 2026-09-03 - session 6: S2b - the capture cost, the lanes, the root, the second draw

Branch `claude/s2b-stereo-scene-draw-a341c5`, seven commits on `VR-Main` (24b22390): the
capture cost measured and the modes, the pipelined deferred capture, positional tracking on
the camera seam, the projection claim and the FOV handoff with the state-gate fixes, the
root derivation and the second draw, the docs. Runs on the dev PC (simulator lane, logs in
`D:\dvr-data\logs\42-run*.log`):

| Run | What | Result |
|---|---|---|
| 16 | capture modes | probe: shared REFUSED; sync 5.4 ms, deferred (first form) 5.2 ms: no gain; `mono.xrs` PASS both |
| 17 | user-memory surface | REFUSED (D3DERR_INVALIDCALL), fell back to sync |
| 18 | the lock split | sync: lock 2.4-3.1 ms, copy 0.7, upload 1.5; deferred first form: the lock still waits |
| 19 | deferred pipelined | lock 0, total 2.25-2.4 ms; `mono.xrs` PASS |
| 20 | postest | camera lane HONOURED on all axes - on the attract camera (the state mislabel found in run 21) |
| 21 | projection on the mono screen | two views, sensor 137, claim readback; the pictures were the title screen, then the loading screen (DISCARDED there), then the sewers: eyetest 120/120, postest +30.0 in real gameplay |
| 22-24 | the state gate | menu/cine tracking hoisted, main-menu flag, LOADING state, the cinematic latch cleared on a new pawn: title MENU -> quad, load LOADING -> quad, level GAMEPLAY -> projection |
| 25 | 1440x1440 | the game stayed 1920x1080 with ResX/ResY=1440 in both ini places; second aspect open |
| 26 | census + scrapes | PVR from one site once per present; render thread presents; the draw chain to the HUD PostRender |
| 27 | tick chain + probes | both chains under UGameEngine::Tick; the root 0x5fc5b0 named from the bytes at 0x6330da; pe-xref confirms every edge |
| 28 | first light | pulse: 3 second draws at 218-414 us, presents +1 each; `stereo reentry`: L/s 54 R/s 53, pair c5 travel 6.17 uu, no fault; the ring cleared every few seconds |
| 29 | the ring fix + soak | L/s 52 R/s 52 mono 0, ringCleared 0, 90 s clean; `reentry.xrs` 11/11 |
| 30 | **the headset (Quest 3, VDXR, the user)** | the doubling ran (draws 54 = 2nd 54, presents 108, pair 6.08 uu) but L/s=36 R/s=54 mono/s=18: left tags dropped by the position check while walking -> both frames in both eyes; lean reversed, a second motion on pitch (the head's displacement and roll not driven under the projection layer) |
| 31 | the fixes, sim | tags never dropped: L/s 53 R/s 53 mono 0; `reentry.xrs` 11/11; the lean under projection read 13.7 uu for 30 cm (the reference had crept) |
| 32 | the stable reference | 30 cm -> +29.4 uu held for 10 s, crouch/forward signs right, postest HONOURED; CINEMATIC stuck across the load (the pawn latched before the title toggle) |
| 33 | the cinematic latch | `cine: latch cleared - leaving the main menu`, LOADING -> GAMEPLAY, L/s 53 R/s 53 mono 0 |
| 34 | HEADSET (the user) | stereo good; tilt and lean reversed in all four directions, also with `stereo projection on` |
| 35 | the lane picture test | 2 m right / 2 m up on both lanes: the camera lane MIRRORED the vp lane on both axes - the field's sign |
| 36 | sign +1 | both axes match the vp lane by picture; eyetest HONOURED 119/120 (c5 -99.2 for +100), postest HONOURED both axes, L/s 52 R/s 51 |
| 37 | roll by picture | roll write lands (incoming = wrote); +20 right-ear-down leaned the verticals RIGHT - reversed |
| 38 | roll negated | +20 leans left, -20 right; forward axis matches the vp lane; pause/resume re-pairs cleanly (L/s = R/s, mono 0) |
| 39 | the verdict logger | `gameplay verdict: FALSE (menuOpen) ... -> the head-locked quad` 30 ms ahead of the runtime's own line |
| 40 | **HEADSET (the user), the verdict** | PASS: stereo depth, tilt, lean, look, crouch all correct. Open: the arming glitch (right eye), judder on fast movement, the pitch pivot behind the camera, and the F10 resolution picker + arming tickbox |

### 2026-09-02 - session 5: the state as session 5 left it (archived)

**The render is restarted on a native D3D9 game.** The DXVK fork, the side-by-side present
pipeline, the 4032x2268 window machinery, the OpenVR backend and the mod's own OpenXR
loader/pace thread/input are removed (one commit each, so `git revert` restores one piece;
history keeps the fork under the `dxvk-*` tags). The BioShock trilogy mod's OpenXR runtime
layer is the single backend (`core/vr/openxr_runtime`, verbatim behind two D3D9 seams: the
device provider and the frame texture), the static Khronos loader is linked into `d3d9.dll`,
and SteamVR rigs go through the bundled `dvr_steamvr32.dll` shim. Stereo is a SEAM with named
methods (`core/gfx/stereo.h`: `[Stereo] Method=mono|aer|reentry`, `stereo <name>` live):
the mono screen (rung 1) works, `aer` and `reentry` are registered design stubs with their
notes. The per-eye camera seam (`game/dishonored/camera`) carries rotation (measured), FOV
(measured) and the lateral eye offset (unmeasured, with the `camera eyetest` instrument).
Version 41.0.0, `[Meta] Version=10`. ARCHITECTURE and ROADMAP (S0-S3) describe it.

**Verified on the dev PC (the game IS installed here, `D:\SteamLibrary`), simulator lane**,
build `g4fb67333` and later, 2026-09-02 evening, eight runs:

- `xrsim-selftest.ps1` PASS; `xrsim-launch.ps1 -ViaSteam` reaches `xr: instance created on
  runtime 'dvr-xrsim'`, `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`, session FOCUSED,
  `xr: first frame submitted to the headset (1600x900 quad)`, frames advancing at the sim's
  90 Hz (`stereo: beat method=mono out/s=90`).
- `status.json`: `state GAMEPLAY` (the game auto-continues into the last save), `stereo.method
  mono`, `framesOut` advancing, capture bbox `100% x 100%`, 97% non-black, `camera.c5ok true`.
- `stereo aer` / `stereo reentry` refuse with their note and mono keeps running; `stereo mono`
  is a no-op; `camera status` prints.
- `xrsim-shot`: a quad layer whose SOURCE reads 97.2% non-black in BOTH views with a full bbox;
  the composite reads L 37.95% / R 37.98% (world-locked quad, run 7) then L 16.4% / R 16.3%
  (head-locked quad, run 8), no `COMPOSITOR fault` / `APP fault` line. The session-4 black
  left eye did not reproduce; the simulator now attributes it if it does.
- `dump frame` writes `capture_*.bmp` (5.7 MB) and `eye_*_mono.png`; `soak.ps1 -Minutes 3`
  exit 0 (PASS, no wedge, no dumps); the crash file carries the run headers.
- `camera eyetest 100` in gameplay: run 7 wrote nothing (the lever off = no camera revalidation;
  fixed), run 8 wrote all six candidates and measured a CONSTANT offset between the draw's c5
  and each field (+6620 uu for 0x80/0x90/0xc4, +14140 uu for 0x330/0x350/0x374 along right):
  the fields are not c5's quantity in c5's frame, so the measure was redesigned around a
  per-candidate c5 baseline (commit `cf9ec6f2`). Runs 10-11 (after the stuck process cleared
  on its own, no reboot): **camera+0x330 HONOURED 119/120** (+99.2 uu of the asked +100; it
  holds -c5 exactly, so the write is negated), the other five DISCARDED. The eye-offset write
  point is measured; `[Camera] EyeField=0x330` is the default (ENGINE_NOTES has the table).
- `mono.xrs` PASS (both eyes 12.9%, equal bboxes, no fault line) and the head-lock pair on the
  fixed simulator: the composite bbox is IDENTICAL at yaw 0 and yaw 30 (run 9).

**Found on the way** (each fixed in its own commit, all measured, none guessed):

1. The game calls `Direct3DCreate9` twice; the second `init_instance` failed with
   `XR_ERROR_LIMIT_REACHED` and the fallback chain declared VR off (guarded).
2. The handoff's trap 6 is real: a DIRECT exe launch crashes at the main menu
   (`Dishonored.exe+0x60907e` reading NULL, thread "other", right after
   `DisGFxMoviePlayerMainMenu Start`); a Steam launch survives it. `xrsim-launch.ps1 -ViaSteam`
   exists for this and is the only way to run the simulator with the game here.
3. The agent's shell on this PC VIRTUALIZES writes under the user profile: files the harness
   wrote to `%LOCALAPPDATA%\DishonoredVR` (the sim manifest, `command.txt`) existed for the
   shell and a game it launched directly, and not for a game launched through Steam (its
   listing held only game-written entries; a WMI-created `dir` agreed). `[Paths] DataDir=` in the
   ini and `DVR_DATA_DIR` for the scripts point both at `D:\dvr-data`; the simulator takes its
   state dir from the manifest's directory (VERIFICATION gotcha 14).
4. The loader's property store beats the environment: `init_instance` hands `[VR]
   XrRuntimeJson` to `xrInitializeLoaderKHR` (XR_EXT_loader_init_properties) as well, and logs
   whether the manifest is readable and its library loads.
5. The config's version rewrite dropped `[VR] XrRuntimeJson`; it now carries `XrRuntimeJson`,
   `Runtime` and `DataDir` over.
6. The fresh ini armed `FovLever=130` (the side-by-side value) and wrote it 600 times per 3 s
   into a 90-deg camera; the lever ships off on the mono screen.
7. The mono quad sat in BioShock's world-locked LOCAL space; it is head-locked now
   (`[Screen] HeadLocked=1`).
8. The SIMULATOR composited quads in the wrong place (60 px outward per eye at yaw 0, 460 px
   of swing at yaw 30): the cbuffer matrix was read column-major and the view matrix's rotation
   block was transposed. Fixed (`d43eea11`), selftest PASS, and the re-measure passed (run 9:
   identical bboxes at yaw 0 and 30). BioShock's eye legs never saw it (projection layers
   rotate rays in the shader).
10. The c5 capture only caught an upload STARTING at register 5; after the two device Resets
   a level load brings, the engine batches it into a c0 x128 block and the seam saw no c5 for
   a run (run 9). Any block covering c5 feeds it now (`dd10da09`).
9. Quitting: `console exit|quit` returns -1 (the console seam does not reach a quit); WM_CLOSE
   logs `ViewportClosed` and then the process LINGERS with one thread, unkillable (no `PreExit`,
   no `proxy unloading`), which then holds `d3d9.dll` and the simulator DLL open and makes Steam
   refuse a relaunch. This ended the session's runs; a reboot clears it. Run 6 also logged an
   access violation inside `d3d9.dll+0x87c95` (VR disabled, right after a device Reset following
   a `GetRenderTargetData` failure on a multisampled backbuffer), thread "other", three times at
   page ends 5.3 MB apart; the process survived it. Unsymbolized (that build is gone); the Reset
   + AA path is the first suspect.

**Headset: verified** (run 13, 2026-09-03): Quest 3 through VirtualDesktopXR, the game on the
head-locked screen in both eyes, head tracking and the gamepad working. The quit crashed on
that run (the 38.79 class: VD's thread through a freed d3d11 pointer after PreExit with the
session still open). The handler had sat inside the motion-aim block since 38.79 and never
ran under GamepadOnly; hoisted, it closes the session from PreExit and the third quit (run
15) was clean: `shutdown: game PreExit`, `xr: session teardown`, `instance destroyed`,
`proxy unloading`, no exception.

**Not verified**: the SteamVR shim with
Dishonored; `apply_eye_offset` driving a real per-eye render (no method asks for an eye yet);
`head_track`/`pad_bridge` as real modules (deferred, S1).

#### Session 5 next steps (superseded by the list above)

**Both**: the recipe on this PC is `tools\build.ps1; tools\install.ps1;
$env:DVR_DATA_DIR='D:\dvr-data'; tools\xrsim-launch.ps1 -ViaSteam` (the game ini carries
`[Paths] DataDir=D:\dvr-data`), foreground the window, `tools\xrsim-run.ps1 -Path
tools\xrsim\mono.xrs -Dir D:\dvr-data\xrsim`. Close the game with Stop-Process while it is
healthy, never with WM_CLOSE (blocker below). Copy `dishonored_vr.log` out before every
relaunch. The eyetest is done: the eye offset writes into camera+0x330 in negated form
(`camera::apply_eye_offset`); `camera eyetest 100` re-measures it on any build.

**Developer A (AlternateEye, S2a)**: read `core/gfx/aer.cpp`. The eye field is measured
(0x330), so the method only has to alternate `eye_for_next_frame()` and tag each present; the
seam writes the offset on the script lane. Acceptance: `stereo aer`
accepted, the beat line `L/s == R/s == out/s / 2`, `stereo.xrs`, `eye-check.ps1` legs 0-5, the
runtime's pair probe clean.

**Developer B (SequentialReentry, S2b)**: read `core/gfx/reentry.cpp`. Task one is the
scene-draw root (caller census at `ApplyHeadToViewRotation`, live stack scrape, identify the
pass by making it MOVE with the eyetest as the mover); every address to `patterns.h` with its
derivation in ENGINE_NOTES; the second call deny-by-default and SEH-guarded.

**The user**: the headset run on Quest 3 via VDXR: `tools\install.ps1`, launch through Steam
with VD streaming and VDXR active (SteamVR not running), expect the game on a head-locked
screen in both eyes, head rotation turning the view, the gamepad working; F10 for the screen
size; send `dishonored_vr.log`. Quit through the game's own menu and report whether the process
lingers.

#### Session 5 blockers (superseded)

- **WM_CLOSE leaves a stuck `Dishonored.exe`** (one thread, unkillable, holds `d3d9.dll` and
  the build's `dvr_xrsim32.dll`, Steam refuses a relaunch). Pid 13452 cleared on its own after
  about an hour, no reboot. Until the quit path is understood, close a healthy game with
  `Stop-Process`, which works.
- **The quit path**: `console exit` returns -1; WM_CLOSE leaves the process lingering with no
  `PreExit`; the runtime layer's teardown therefore never runs on a graceful close. Which
  thread is stuck (the simulator's, the runtime's, a driver's) is unknown; a debugger on the
  next occurrence, or a minidump taken by hand before killing it.
- The headset run needs the user.

### 2026-09-02 - session 5: the native-stereo foundation (41.0)

The decision (docs/ARCHITECTURE.md decision log, session 5): four headset sessions showed the
DXVK side-by-side design cannot be tuned; the game renders natively again and stereo is rebuilt
as a ladder of methods on one seam, two developers taking rungs 2 and 3. One PR
(`claude/native-stereo-foundation-77e2b6` -> `VR-Main`), 27 commits: seven removals, the
static loader, the runtime layer, the shim, the stereo seam + mono screen, the camera seam +
eyetest, the stubs, the simulator instruments, the harness, the docs, then the fixes the
first runs demanded (above, "Found on the way").

Runs on the dev PC (simulator lane; logs in `D:\dvr-data\logs\41-run*.log`):

| Run | Launch | Result |
|---|---|---|
| 1 | direct exe | instance on dvr-xrsim, then init_instance twice -> VR off; the game CRASHED at the main menu (`Dishonored.exe+0x60907e`, trap 6) |
| 2 | Steam | the ini rewrite dropped XrRuntimeJson -> VDXR (no headset), flat |
| 3 | Steam | env var set but the loader answered RUNTIME_UNAVAILABLE |
| 4 | Steam | loader property override set; manifest "path not found" (err 3) - the sandbox finding |
| 5 | Steam | the path probe: the game sees 5 entries where the shell sees 7 |
| 6 | Steam (no VR) | an AV in `d3d9.dll+0x87c95` after a Reset + RTD failure, three times, survived |
| 7 | Steam, `D:\dvr-data` | **dvr-xrsim, FOCUSED, first frame submitted, GAMEPLAY, both eyes 38% non-black, soak PASS 3 min**; eyetest NOT WRITTEN (null camera) |
| 8 | Steam | head-locked quad 16% per eye but swinging with yaw (the sim's quad math); eyetest wrote, measured the field/c5 offsets; WM_CLOSE -> the stuck process |
| 9 | Steam (after the process cleared) | `mono.xrs` PASS; head-lock pair IDENTICAL bboxes on the fixed sim; no c5 (the c0 x128 block) |
| 10 | Steam | c5 back; eyetest: 0x330 reads -c5 and moves c5 by -98.7 uu (75/76), the rest discarded |
| 11 | Steam | sign-aware seam: **0x330 HONOURED 119/120 (+99.2 uu)**, five DISCARDED; `camera eyefield 0x330` |
| 12 | Steam + Quest 3 (VDXR) | flat: a stale `[VR] XrRuntimeJson` (the sim manifest) made the loader fail; fixed to warn and ignore (`21e1cb64`) |
| 14 | Steam + Quest 3 (VDXR) | quit crashed again: the PreExit handler never ran (it lived inside the motion-aim block, off under GamepadOnly) - hoisted (`cf506ba4`) |
| 15 | Steam + Quest 3 (VDXR) | **clean quit**: `shutdown: game PreExit`, session teardown, instance destroyed, proxy unloading, no exception |
| 13 | Steam + Quest 3 (VDXR) | **THE HEADSET RUN: VirtualDesktopXR, Meta Quest 3, FOCUSED, READY, the screen in both eyes following the head, the gamepad working (user's report)**; quitting through the menu crashed 2.3 s after PreExit (EIP DEDEDEDE in d3d11.dll on VD's thread, the session still open) - teardown moved to the PreExit handler |

### 2026-09-02 - session 4e: gamepad-only, and the three rendering symptoms

Headset run at 4032x2268 requested / `capture: 3840x2160` actual. The tester reported three
things and they turn out to be one geometry. Full derivation in ENGINE_NOTES, "The three
rendering symptoms, and the one geometry that ties them".

1. **"Super pixelated, but the pause menu is huge like it's at full resolution."** Both
   halves are the same fact: SBS gives the WORLD half the frame width per eye
   (`per eye 1920x2160`) while a MONO menu frame samples the whole 3840 across the same quad.
   The menu is drawn at exactly twice the world's horizontal sampling density. That is the
   cleanest confirmation of the SBS packing anyone has produced, and it is not a bug - but it
   means the frame must be at least `2 x eyeWidth = 4992` columns for a 1:1 world. At 3840 the
   world sits at 77% of the panel.
2. **The fisheye is `FovLever`.** It does not only size the quad, it WRITES the game camera's
   FOV (`fov_lever.cpp`), so `FovLever=130` makes the game render 130 deg horizontal - the log
   agrees (`MEASURED render FOV ... = 130.0 deg`). A 130 deg rectilinear frame shown across a
   94 deg frustum stretches the edges. The author already knew: `frame_hooks.cpp` disarms the
   lever on overshoot, commented *"rather than leave the user in a fisheye"*.
3. **The black bottom border cannot be tuned away at 16:9.** Filling a 99 deg vertical
   frustum needs `lever = 2*atan(tan(v/2)*aspect)`: 128.6 at 16:9, 114.6 at 4:3, 100.5 near
   square. So the fisheye and the border are the SAME setting pulled in opposite directions,
   and at 16:9 nothing satisfies both. The tester found that empirically. A taller frame is
   not a preference, it is the only way out - which is exactly what they asked for.

**Next single change: `3840x2880` (4:3) with `FovLever` ~115.** Same per-eye width as now, so
no sharpness regression, +33% pixels, and it should visibly ease the fisheye.

**Two corrections to 4c, both mine.** (a) "Must be a real display mode" was too strong -
3840x2160 WAS honoured. The real rule is narrower: **`PinBackbuffer=1` causes the crop**; a
size the game rejects merely falls back, harmlessly, as long as the pin is off. Both effects
were present at 2850x2750, which made them look like one. (b) There is no 2560x1440 cap -
that was read from the run before the pin was turned off. 4032x2268 is still not honoured
(empty `setres` replies), so **trust `capture:`, never the requested number**.

**Applied this session:**

- **`[Mode] GamepadOnly=1`, new and default ON** (`config.cpp`). Turns off SkelControl hand
  writes, hand mesh, motion aim, motion melee, motion crouch and controller Blink aim, and
  scales no hand or weapon model. Head tracking, positional tracking, the FOV lever and the
  virtual gamepad keep running - this is NOT the `XR_SAFE` bisector, which also stops the
  head. It logs loudly and `status.json` gains `gamepadOnly` so the zeroes below it read as
  BY DESIGN rather than as failures. The author's rule that motion crouch and hands "must
  never stop working" is respected: nothing is retired, it is one key, set `GamepadOnly=0`.
- **The tester's tuned values are now the repo defaults**, in both the generated ini text and
  the `IniFloat` fallbacks, so a fresh install comes up where the headset testing left off.
- **`[PosTrack] Scale` default 50 -> 98.** This closes 40.2b: the tester tuned world scale by
  feel and landed on 98, within 2% of the 100 derived from the movement constants, arrived at
  independently and without seeing the number. That is the cross-check 40.2b was waiting for.
- Hand trims and `HandSize` reset to neutral, per the tester's "no scaling or changing the
  default hand/weapon models".

Build clean, exports 9/9 undecorated, lint clean, RelWithDebInfo installed. The fork and
`dxvk_stereo.txt` are untouched.

### 2026-09-02 - session 4d: the lever is half of the resolution setting

**The 3840x2160 run was full-frame but letterboxed** - tester: "almost sort of right again,
only problem was that the resolution was rectangular so it didn't fill my view". Both halves
of that are now explained, and one of them was my error.

**My error.** Session 4's restore set `FovLever=100`, correct for the near-square 2850x2750
it was paired with. Session 4c then changed the render to 16:9 and left the lever at 100.
The frustum-fill branch takes its vertical extent as `tan(fovDeg/2)/aspect`, so at 16:9 with
lever 100 the quad clamps to **67.7 deg inside a 99 deg frustum** - letterboxed by
construction. At lever 130 the same 16:9 render fills edge to edge with ~9% black at the
bottom. Changing the render aspect without changing the lever is NOT a one-variable change:
the pair is the variable. Table in ENGINE_NOTES, "FovLever IS the vertical fill lever".

**The requested resolution is not being honoured at all.** The mod asked for 3840x2160; the
log says `capture: 2560x1440`. With `PinBackbuffer=0` that is harmless - buffer and content
agree, no crop, which is why 4c's fix worked - but **this rig cannot render above 2560x1440**
(desktop 5120x1440 caps it). Every "4032x2268" run here is really 2560x1440. Trust the
logged `capture:` number, never the requested one.

**F10 SAVE AS DEFAULTS was finally pressed** (the thing session 3c said had never happened).
The tester's tuning is now persisted and snapshotted to
`tests/golden/f10-tuned-2026-09-02.ini` and `<game>\...\dishonored_vr.ini.f10-saved-0247`:

| key | default | tuned |
|---|---|---|
| `[Tracking] HeightOffsetM` | 0.000 | 0.040 |
| `[Screen] FillScale` | 1.00 | **0.74** |
| `[Screen] DistanceMeters` | 1.60 | 1.67 |
| `[PosTrack] Scale` | 50.0 | **100.0** |
| `CrouchToggle` | 1 | 0 |
| `XrLayer` | (absent) | proj |
| `StampFix` | (absent) | 0 |

`[PosTrack] Scale=100.0` matches the measured 100 uu/m from commit 60235b86 - the tester
converged on the measured value by feel, which is a good cross-check on that measurement.

**Applied on request: GingasVR's resolution default**, as the coherent pair -
`RenderWidth/Height=4032x2268`, `SpoofDesktopW/H=4096x2304`, **`FovLever=130`** - plus
`DishonoredEngine.ini` and all four AppCompat buckets at 4032x2268. `PinBackbuffer` stays 0
(her ini has no such key) and `MenuFillScale` stays 1.00 (the 4b fix). Every F10 value above
is preserved. Backups `.pre-gingasres` / `.f10-saved-0247`. NOT YET TESTED.

**Expect this**: the render will probably clamp to 2560x1440 again (fine, still 16:9), and at
lever 130 the quad should fill horizontally and to the top with ~9% black at the bottom -
**but `FillScale=0.74` will still present it at 74% of that**. F2 raises FillScale live in
the headset; that single knob is the difference between 74% and full. Judge the lever first,
then the fill.

### 2026-09-02 - session 4c: THE RENDER WAS NEVER 2850x2750

**This is the root cause.** The tester sent a desktop-mirror screenshot of the main menu with
the picture in the top-left of the window. The mirror blits the backbuffer's LEFT HALF
pillarboxed (`frame_hooks.cpp:415-460`), so its horizontal placement is expected - but the
picture filled only the top ~52% of the window, and that is not.

**Measured, six capture dumps, non-black bounding box:**

| requested buffer | actual content | real display mode? | verdict |
|---|---|---|---|
| 1600x900 | 1600x900 | yes | FULL |
| 2560x1440 | 2560x1440 | yes | FULL |
| 3840x2160 | 3840x2160 | yes | FULL |
| 4032x2268 (GingasVR's) | 3024x1440 | no | CROPPED |
| 2750x2850 | 2750x2200 | no | CROPPED |
| **2850x2750 (our "known good")** | **2560x1440** | no | **CROPPED** |

At 2850x2750 the game draws exactly **2560x1440 into the top-left** and leaves the rest
black. So the whole session's geometry was applied to a frame that is half empty. It
explains all three symptoms at once: tiny (content covers ~90% x 52% of the quad), top-left
(it is literally there), and the eyes not fusing (the SBS halves meet at x=1280, not the
x=1425 the split assumes, so each eye gets part of the other's view plus black).

**Why nothing caught it.** `PinBackbuffer=1` forces the DEVICE to 2850x2750 while the game
renders at the size it asked for (`CreateDevice the game asked for 2560x1440`). The mod then
spoofs `GetClientRect` to 2850x2750 and the setres path reads that spoof back, concluding
`setres: the game is already at 2850x2750 - skipping the resolution script entirely`. A
check reading our own spoof cannot fail its own hypothesis, so the engine-side resize never
ran and `capture: 2850x2750` was logged for a half-empty frame.

**`PinBackbuffer` is ours, not GingasVR's.** Her tuned ini (`.pre-2750`) has no such line.
Every ini since session 2 sets it to 1.

**This is probably the central open bug.** 4032x2268 is not a standard mode either and
cropped here too. Whether an injected mode is honoured depends on the machine's GPU, driver
and desktop mode - this rig's desktop is 5120x1440, and two of the three cropped captures
came back exactly 1440 tall. It predicts affected users have a desktop shorter than the
requested render height, and it is falsifiable by asking one for their desktop resolution.

**The Documents folder was ruled out**, on the tester's suggestion: `DishonoredEngine.ini` is
vanilla plus the intended VR lines, all four AppCompat buckets were already correct, and no
file in the Config directory contains 2560 or 1440.

**Applied for testing** (one coherent change: use a resolution the display actually offers):
`RenderWidth/Height 2850x2750 -> 3840x2160`, `SpoofDesktopW/H -> 3840x2160`,
`PinBackbuffer 1 -> 0`, plus `DishonoredEngine.ini` and all four `[AppCompatBucket1..4]` to
3840x2160. Per-eye half 1920x2160 = aspect 0.889, the same per-eye aspect as GingasVR's
4032x2268. Backups: `.pre-realmode` next to each of the three files. NOT YET TESTED.
Fallback if 3840x2160 is not honoured: 2560x1440, identical per-eye aspect, and the game
asked for it itself.

**Also confirmed this session**: the 4b MenuFillScale fix works. The 02:35 run logged 28 quad
rebuilds, all at `fill=1.00` / 100.0 x 98.0 deg, none at 0.60. The size pumping is gone.

**Next instrument to build**: nothing compares the captured frame's real content extent to
the buffer size. A non-black bounding-box check on the capture, logged once per resolution
change, turns this class of bug into one line. The setres check must also stop reading the
mod's own `GetClientRect` spoof.

### 2026-09-02 - session 4b: the world size PUMPS, and MenuFillScale is why

A headset run on the restored known-good ini (02:23, VirtualDesktopXR + Quest 3) reported
"still rendering tiny and in the top left corner". Its log names the cause outright, so this
did not need a new instrument. Full derivation in ENGINE_NOTES, "MenuFillScale pumps the
world size during GAMEPLAY".

**The number.** The quad subtends **71.1 x 69.2 deg** inside a frustum of **94.0 x 99.0 deg**
- about half its solid angle. That is "tiny", fully explained, and nothing to do with
resolution, adapter, world scale or convergence. 40 of the run's 46 quad rebuilds were at
`fill=0.60`; the run ended there.

**The mechanism.** `MenuFillScale=0.60` and the `XrFrustumFill` gate are driven by the SAME
condition (`g_menuOpen || g_inMenu || g_sbsMonoNow`), and a change in it forces a rebuild.
The menu flag flaps during gameplay (the `Req_SaveSlotInfos` save-slot polls, already
documented at `present.cpp:613-616`), so the world size pumped 100 -> 71 -> 100 -> 71 -> 100
-> 71 deg across the six seconds before the crash, all after gameplay had started. The
`sbs:` line proves it is the MENU flag and not the splice counter: its last transition is
well before the pumping began.

**Why the Index never saw it.** OpenVR has one geometry path, so `MenuFillScale` only ever
dimmed a menu. `XrFrustumFill` (38.13) added a second path for the OpenXR port without making
the transition continuous, so on Quest the same flap swaps the whole quad construction
mid-gameplay. The tester's own read - Index/SteamVR was the tuned target, OpenXR/Quest a
later port - is exactly right here.

**Applied, config only, one variable, no rebuild**: `[Screen] MenuFillScale 0.60 -> 1.00`
(backup `.pre-menufill`). The menu branch now builds the same 100.0 x 98.0 deg quad as
gameplay, so a flap cannot change the world size. Cost: menu edges crop, which is what 32.4
added the key to avoid. NOT YET TESTED.

**A falsifiable prediction, and it contradicts session 3c.** Worked from the logged frustum,
the authored quad's vertical border must be SYMMETRIC, ~21% black top and ~21% bottom, with
the world in the middle 57.6%; horizontally the left eye gets 29.8% black on the temple side
and 5.6% on the nasal side (mirrored in the right eye - the rigid-screen design). Session 3c
recorded "top ~54%, bottom half black". The next `dump eyes` settles it: symmetric borders
retire that contradiction as a misread dump; a real black bottom half falsifies this model.

**The run also CRASHED** at 02:23:49, wild instruction pointer, minidump at
`%LOCALAPPDATA%\DishonoredVR\dumps\dvr_20260902_022349.dmp`. Untriaged, separate lane.

**Good news in the same log**: the fork's projection export resolved this time -
`quad/fill: world scale is set by the MEASURED render FOV (fork dxvk_vr_proj) = 100.0 deg`.
The landscape fix (session 3b) worked; world scale is no longer an assumed constant.

### 2026-09-02 - session 4: reverted to the known-good point

No launches, no code changes. Session 3c left the rig one key away from its own
confirmed-good configuration and that key was an open, unevaluated experiment; this
restores the documented point so the next run starts from a known baseline.

**What was actually different.** Exactly one line: `FovLever=130` vs `100`. Everything
else already matched - `RenderWidth/Height=2850x2750`, `SpoofDesktopW/H=2816x2880`,
`PinBackbuffer=1`, `GameFOVDeg=100`, `FillScale=1.00`, `[PosTrack] Scale=50.0`, the game
ini at 2850x2750 on both `[SystemSettings]` and `[SystemSettingsEditor]`, and all four
`[AppCompatBucket1..4]` at 2850x2750. `setup-game-ini.ps1` did not need re-running.

**The snapshot checks out.** `tests\golden\known-good-2850x2750-lever100.ini`, the game
folder's `dishonored_vr.ini.KNOWN-GOOD` and `dishonored_vr.ini.pre-lever130` (the ini as it
actually ran when the tester reported "the eyes seem to overlap correctly and provide
depth") are all identical. The snapshot is a truthful record of the run, not a
reconstruction - worth stating, because it was written at 00:54 while the live file was
already at `FovLever=130`.

**Restored** by byte copy, verified with `cmp` against both snapshots. Prior state saved as
`dishonored_vr.ini.pre-restore-known-good`.

**`FovLever=130` is untried, not disproved - and it was the well-motivated direction.**
ENGINE_NOTES "FovLever and the render size are ONE setting" has it the other way round from
how session 3c's ordering reads: at lever **100** the clamp limit is 1.91 m against a
frustum reaching 2.20 m horizontally and 2.29 m vertically, so it fires on **all four
sides**, and `dump eyes` at lever 100 confirmed the world inset with a ~9-10% border on
every side. At lever **130** the limit is 3.43 m, outside the frustum edge, so nothing
clamps and the quad fills the eye. 130 is also GingasVR's own tuned value.

**So the restore knowingly reinstates the bordered configuration.** That is the right call -
lever 100 at 2850x2750 is the only point a tester has ever confirmed fuses with depth, and
an unevaluated experiment is not a baseline - but the border is a KNOWN artifact of this
baseline, not a new symptom, and "the render window is halfway up my vision" must be judged
against that. Lever 130 stays queued as the next one-variable change once the gameplay dump
is in hand.

**Caveat that still stands**: no F10 tuning has ever been persisted (SAVE AS DEFAULTS was
never pressed), so this ini is the only reproducible configuration that exists.

**Untouched deliberately**: the proxy, the fork, `dxvk_stereo.txt`, and the stale
`command.txt` in `%LOCALAPPDATA%\DishonoredVR\` (`command.cpp:153` discards and clears a
command file older than the process, so the next run's log should show that branch firing -
a free check of the new seam diagnostics).

**Next**: unchanged from session 3c. Launch, reach GAMEPLAY with the window focused, then
`tools\game-cmd.ps1 "dump eyes"` and read the two PNGs. The unexplained contradiction is
still the lead: the world occupied only the top ~54% of the eye render target while the
measured frustum (55 deg down against 44 deg up) says the quad should overflow vertically.
That dump was a MENU frame and needs confirming in gameplay before anything is changed.

### 2026-09-02 - session 3c: the seam went deaf, and the eye texture is half black

**START HERE.** The bug is not fixed and the last measurement is incomplete.

**The one thing to do first**: launch, get into GAMEPLAY (not a menu, window focused),
then `tools\game-cmd.ps1 "dump eyes"` and look at the two PNGs in
`%LOCALAPPDATA%\DishonoredVR\dumps\`. Everything below is waiting on that image.

**The live symptom** (tester, Quest 3 + VirtualDesktopXR): eyes will not fuse, and the
image sits high - "the render window is halfway up my vision so I only see the bottom
half of it". Earlier in the session, "everything looked tiny".

**The measurement that matters.** A `dump eyes` caught mid-session shows the world
occupying only the **top ~54% of the eye render target, bottom half pure black**, in both
eyes. That is our own D3D11 pass, before the compositor. It was a MENU frame (the dump
caught a paused game), so it needs confirming in gameplay - but the vertical placement is
the lead. The measured frustum is
`eye frustums: L[-1.376 0.839 -1.428 0.966] ex=-0.0316 | R[-0.839 1.376 -1.428 0.966]`,
slots `[left, right, down, up]`: **down-biased, 55 deg down against 44 deg up**. Worked by
hand at these settings the quad should span y -2.36..+1.62 against a frustum of
-2.29..+1.55, i.e. it should OVERFLOW the eye vertically, not sit in the top half. That
contradiction is unexplained and is the next thing to chase.

Same dump showed the two eyes holding **different content** (different horizontal extents,
different fragments of the same menu text). That is the mono/stereo UV race on menu frames
- the fork stops splicing, the frame goes mono, each eye still takes its own half. Probably
menu-only: in gameplay the splice count is 5000+ and the halves are a real stereo pair.

**The command seam went deaf and blocked the session.** `status.json` stale for 18 minutes
and `command.txt` unread, while the Present hook ran at 62 fps with 251 `[present]` lines.
Two `dump eyes` commands did nothing and left no trace. `poll()` had FIVE silent returns,
so the failure was indistinguishable from the command never being written. **Fixed and
installed**: each guard now names itself with the path, the size and GetLastError. If the
seam is still deaf next session the log will say which branch refuses.

**Theories killed this session (do not re-run them):**

- The `CopyResource` size mismatch (step 0a) - eye RTs and XR eye size matched exactly.
- The pace thread as the crash victim - the faulting thread is `(other)`, not `xr-pace`.
- The black left eye - a SIMULATOR defect, not the mod; `dump eyes` shows the left eye
  texture full. See VERIFICATION "Known simulator defects".
- Eye cant - `g_eyeRot` is declared identity and the XR path correctly leaves it alone.
- The clock/rate gate - `MaimNowMs` is fine (the 5 s `depth:` line printed 50 times against
  the 3 s heartbeat's 81). Note the log uses `GetTickCount` while the gates use QPC via
  `dvr::clock`, so an advancing log proves NOTHING about the gate clock.
- A game restart clearing the seam - it did not.

**THE KNOWN-GOOD STATE, AND HOW TO GET BACK TO IT.** This rig's best result so far -
tester: *"the eyes seem to overlap correctly and provide depth, there is no freeze"* - came
from `2850x2750` + `FovLever=100` + `Scale=50`, with the main scene splicing (5202). It is
worth more than GingasVR's own values, which come from a different machine and are the
subject of the project's central open bug ("works only on her PC").

Snapshotted byte-for-byte in two places, so it survives a wiped game folder:

- `tests\golden\known-good-2850x2750-lever100.ini` (in the repo)
- `<game>\Binaries\Win32\dishonored_vr.ini.KNOWN-GOOD`

Restore = copy either over `<game>\Binaries\Win32\dishonored_vr.ini`, then
`tools\setup-game-ini.ps1 -Resolution -Width 2850 -Height 2750` for the game ini and the
four AppCompat buckets. The keys, if you ever need to rebuild it by hand:
`[Screen] RenderWidth=2850 RenderHeight=2750 SpoofDesktopW=2816 SpoofDesktopH=2880`
`PinBackbuffer=1 GameFOVDeg=100 FovLever=100 FillScale=1.00`, `[PosTrack] Scale=50.0`.

**WARNING - the snapshot does NOT contain any F10 tuning, and neither did any run.** The
overlay's sliders are live-only until someone presses **"SAVE AS DEFAULTS"**
(`overlay.cpp`, top of the panel), which calls `OverlaySaveDefaults` and writes ~90 keys -
world scale, fill, screen distance, height offset, menu fill, wrist HUD, and the whole hand
/ graft / blink block. It was never pressed, so every F10 adjustment the tester made across
this session was lost at process exit; the only thing that persisted was the hand
calibration, which `skelcontrol.cpp` writes on its own. **Procedure from now on: tune in
F10, press SAVE AS DEFAULTS, then re-snapshot the ini.** Otherwise a good configuration
cannot be reproduced, which is exactly what happened here.

**Config as left, LIVE right now**: ~~`FovLever=130`~~ **RESTORED to the known-good
snapshot (session 4)**. The live `dishonored_vr.ini` is now byte-identical to both
`tests\golden\known-good-2850x2750-lever100.ini` and the game folder's
`dishonored_vr.ini.KNOWN-GOOD` (`cmp` clean against both), i.e. `FovLever=100`. The
`FovLever=130` experiment was applied but never evaluated - the seam went deaf before a
dump could be taken - so it was discarded rather than judged; it remains untried, not
disproved. The pre-restore state is saved as `dishonored_vr.ini.pre-restore-known-good`.
Game ini + all 4 AppCompat buckets verified still at 2850x2750 (`DishonoredEngine.ini`
lines 1081/1141, `DishonoredCompat.ini` buckets 1-4), so no `setup-game-ini.ps1` re-run
was needed.
Backups in the game folder: `.pre-2750` (GingasVR's own tuned ini), `.pre-landscape`,
`.pre-scale100`, `.pre-gingas-restore`, `.pre-rollback`, `.pre-lever130`,
`.pre-restore-known-good`.

**Installed**: RelWithDebInfo proxy only, 00:50, carrying the seam diagnostics. The fork
(20:24) and `dxvk_stereo.txt` (13:52) are untouched and must stay that way - one variable.
Re-verified session 4: the installed `d3d9.dll` is md5-identical to
`build\src\RelWithDebInfo\d3d9.dll`, and the tree is clean at `112105b7`, so source,
build and install all agree. The fork and `dxvk_stereo.txt` timestamps are unchanged.

**Do not repeat these mistakes.** Restoring GingasVR's baseline I changed render size, FOV
lever and world scale in ONE step, so "misaligned" was unattributable; her values also come
from a different machine, and the project's central open bug is that her build works only
on her PC. This rig now has its own confirmed-good point (2850x2750, splices 5202, tester:
"eyes overlap correctly and provide depth") which is worth more than her numbers. Change
one thing per run, and get the gameplay dump before changing anything at all.

### 2026-09-01 - session 3b: the render was PORTRAIT, so there was no stereo

A headset run mid-session reported "the eyes are suuuuper far off and they both appear to
be zoomed in", worse than before. Its log is the **first surviving headset log** and is
archived outside the game folder. Cause found, fix applied, not yet tested.

**Session 2's resolution fix set the render to 2750x2850, which is portrait, and the DXVK
fork refuses to splice the main scene on a portrait viewport.** `d3d9_device.cpp:4381`
sets the refusal reason `"rt-portrait"`; the per-eye splice at `:4578` runs only when that
reason is `SPLICE`. So the world was drawn **mono** across the full frame while the proxy
handed each eye a different **half** of it - unrelated views that cannot fuse, each
magnified 2x by the stretch onto the quad. Exactly the report.

**The splice counter lies about it.** Light shafts, shadows and the M8.1 quarter light pass
splice under different conditions and kept working, so `splices=85` while the main scene
never spliced once - which kept `g_sbsMonoNow` false and the half-frame UVs on. The fork's
own log shows only effects splices.

**The same gate kills the FOV measurement.** `dxvk_vr_proj`'s publish (`:5996`) is also
gated on `Width > Height`, so `g_liveFovX` was 0 all session, the frustum-fill path fell
back silently to the ini constant `GameFOVDeg=100`, and an assumed number set world scale.

**This falsifies session 2's "the eyes ARE a stereo pair".** 32.7 mean-abs-diff static /
11.5 after a head turn is exactly what two different halves of one mono frame produce. That
test could not distinguish a stereo pair from two unrelated crops, so it could never have
failed its own hypothesis.

**Applied**: `2850x2750` - the same two numbers swapped. Same pixel cost, landscape by
100 px, full-frame aspect 1.036 so the quad subtends 100 x 98 deg at `FovLever=100`.
Changed in `tools/setup-game-ini.ps1` (defaults + a header section on why landscape is
mandatory), applied to `DishonoredEngine.ini` and `DishonoredCompat.ini` via the tool (both
backed up), and to the game folder's `dishonored_vr.ini` (backup `.pre-landscape`).

**Instruments added so this cannot hide again**: a portrait capture logs an Error naming
the fork's own refusal string and the fix (`present.cpp`); the frustum-fill path now says
every 10 s whether world scale comes from the MEASURED render FOV or from the assumed ini
constant (`eye_quads.cpp`).

**Installed**: RelWithDebInfo proxy only (`install.ps1 -Release -SkipDxvk`) - the fork and
`dxvk_stereo.txt` are untouched, so the resolution is the only render-path variable. The
proxy's other changes (session 3 below) are the shutdown/pace lane and logging, which
cannot confound the zoom result.

**Not yet tested.** If the fix worked the portrait Error is absent and the eyes fuse; if
the Error appears, the resolution did not take and AppCompat is overwriting it again.

### 2026-09-01 - session 3: the crash fingerprint was misread, and why

No launches: everything here comes from artifacts already on disk plus the source. The
game is installed on this PC, but nothing was run.

**Two of session 2's conclusions are instrument bugs, not engine facts.**

1. **The exit crash is an EXECUTE fault, not a freed-memory write.**
   `ExceptionInformation[0]` is three-valued (0 read, 1 write, 8 execute/DEP) and the
   fingerprinter tested it for truth, so every execute fault has printed as "writing". The
   records prove it: `ExceptionAddress == ExceptionInformation[1] == 0xDEDEDEDE` with the
   module resolving to `?`. A data write would have left `ExceptionAddress` inside
   `d3d11.dll`. **EIP landed in freed memory: a call through a poisoned code pointer.**
2. **The faulting thread is not the pace thread.** All three records say `(other)`, which
   `thread_name()` returns only for a tid in no registered slot; `present` and `xr-pace`
   both register at entry. The faulter is a third-party worker (d3d11, driver, runtime).
   The pace thread can be the cause, but instrumenting it as the victim will find nothing.

**Two evidence channels were dead and are now fixed.**

- **`dumps\` was empty by construction.** 3 `EXCEPTION` lines, 0 `minidump` lines: proof
  that `unhandled()` never ran, because UE3's own filter/SEH frame consumes the fault
  before `SetUnhandledExceptionFilter` fires. The dump is now taken from the **vectored**
  handler (which always runs), gated on the instruction pointer resolving to no loaded
  module - fatal-only by construction, and falsifiable: an ordinary in-module fault
  produces no dump and disproves the wild-EIP reading. `dbghelp.dll` is resolved at
  `install()` time so the VEH never touches the loader lock.
- **The crash file had no run identity.** `FILE_APPEND_DATA` / `OPEN_ALWAYS` with nothing
  separating runs, and `dvr-xrsim` and VDXR produce byte-identical fingerprint text, so
  the three records cannot be attributed to a backend at all. Now one header per run
  (clock, version, build id, pid, backend + runtime name).
- **Log rotation is one deep**, so two simulator runs erased both headset logs; the
  survivors contain no `EXCEPTION` and no `PreExit`. Copy the log out before each launch.

**The author read this fault correctly and session 2 inverted it.** The 38.79 comments say
"EIP dededede" and "a call through freed memory". 38.79 acted on that by standing the
**game** thread down at `PreExit`, which was right but not the whole path - it left the
pace thread running with nobody waiting for it. Closed below.

**Two pace-lane defects fixed** (steps 0b and 0c):

- **`XR_TIMEOUT_EXPIRED` is a success code.** `XrResult` is negative for failure only, so
  `XR_FAILED()` is false for it and `!XR_FAILED(xrWaitSwapchainImage(...))` ran
  `CopyResource` into an image the compositor had not finished reading - a race with the
  runtime on the one resource the headset displays, invisible because every call returns
  success. Now `== XR_SUCCESS`. In the same block `g_xrpShown` advanced **before** the
  copies, so a frame lost to a timeout was dropped permanently instead of retried; it now
  advances only once both eyes actually received the content.
- **`XrPaceStop()` joins the pace thread**, bounded at 750 ms, replacing the bare
  `g_xrRun = 0`. On expiry the thread is left running on purpose - `TerminateThread` would
  orphan `g_xrCs` and abandon an acquired swapchain image, which is worse than the race -
  and the error line is the instrument: a fault after it means the pace lane is still the
  suspect, a fault without it means the thread was already gone and it is not. The event
  pump's inner `while` now tests `g_xrRun` so an event backlog cannot hold the loop past a
  stop request.

**Changed** (8 files, uncommitted): `src/core/util/crash.cpp` and `crash.h` (three-valued
AV decode; run header; `set_context`; shared `write_dump` with the wild-EIP gate),
`src/core/vr/openxr_backend.cpp` (names the runtime in the crash context),
`src/core/vr/openxr_pace.cpp` (the wait fix, the retry fix, `XrPaceStop`),
`src/game/dishonored/ue3/process_event.cpp` (`PreExit` joins), `src/mod/fwd.h`,
`docs/dishonored/ENGINE_NOTES.md`, `docs/STATUS.md`. Verified: Debug, RelWithDebInfo and
`-Legacy` all build, and both DLLs carry the new strings; `lint.ps1` clean; exports 9/9
undecorated. **Not run in the game, in the simulator, or in a headset.**

**Next**: step 0a - the `CopyResource` size mismatch, which is still only a code-reading
hypothesis. Then the full `xrRequestExitSession` / `xrDestroySession` shutdown.

### 2026-09-02 - session 2: instrumentation, resolution, and a real headset

**First session with the game actually installed and a real Quest + Virtual Desktop
headset on the other end.** Environment: proxy and fork both built from source with
MSVC (meson + ninja + glslang 16.5.0 standalone, no Vulkan SDK); `tools\build-dxvk.ps1`
needed two fixes to run at all on a PC with VS 2026 installed next to VS 2022.

**Fixed and verified**

- **Resolution.** `ResX` in `DishonoredEngine.ini` never held: UE3 AppCompat picks an
  `[AppCompatBucketN]` at startup and writes that bucket's ResX/ResY over
  `[SystemSettings]`. Buckets 3 and 4 ship 1600x900, and any GPU newer than the 2012
  table lands in one, so every modern machine started at 1600x900 forever. Fix: set all
  four buckets; `tools\setup-game-ini.ps1 -Resolution` now does this and defaults to
  2750x2850. Measured before/after: `CreateDevice (1600x900)` -> `CreateDevice
  (2750x2850)`, `capture: 2750x2850` on the FIRST device creation, no setres needed.
- **`setres` is a dead end.** Measured `setres 2750x2850w -> "(empty reply)"` with NO
  device Reset following. New `[Screen] PinBackbuffer=1` (default OFF) sets the size in
  the present parameters at CreateDevice instead. The 32.57 "image in the corner"
  objection is answered by the GetClientRect hook that landed later.
- **World scale.** `W` is pinned to the rendered FOV and `H = W / frameAspect`, so a
  squarer render makes a taller virtual screen than the lenses can show and the player
  sees a magnified middle. 2750x2850 at FovLever=130 subtends 100x132 deg; at 100 it is
  100x102, which matches the headset. FovLever set to 100.
- **The mono/stereo UV race.** `BuildEyeQuads` BAKES the sampling UVs, but the rebuild
  only fired on an aspect change or a menu toggle. `g_sbsMonoNow` flips during gameplay
  whenever the fork's splice count dips, so a mono frame could be sampled with stereo
  UVs and each eye got a different half of one mono image. Now a change in frame kind
  forces a rebuild, exactly like the menu flag.

**Instrumentation added** (see the new "Logging" section in `CLAUDE.md`)

- Full DXGI adapter enumeration, the LUID the runtime asks for, and the adapter read
  back OUT of the finished device with an Error-level mismatch line.
- `RESOLUTION CHANGED MID-SESSION` at Warn, `quad: ... subtends AxB deg` per rebuild
  with a Warn past 110 deg vertical, `res: the game asked for WxH`, and a `skc/gate:`
  line for the hand drive.
- The hands heartbeat now names the OWNER and reports that owner's counter.

**Corrected beliefs** (all three were believed and are wrong)

1. "The hand graft never attaches." It attaches fine: `OWNER=SkelControl writes=~406/3s`
   in gameplay. The old heartbeat tracked two counters that read 0 BY DESIGN - one a
   retired subsystem, one the legacy drive that is deliberately stood down. Three
   readers including the original author concluded "the hands are dead" from a healthy
   run.
2. "The 39.3 adapter bug needs two GPUs." DXGI enumerates the SAME RTX 4070 Ti SUPER
   twice on this PC (virtual display drivers), so the default adapter is not stable on
   single-GPU machines either. **But on the real VDXR run the runtime asked for
   adapter[0], which IS the default, so the LUID mismatch is NOT the cause of the
   symptoms on this rig.** 40.1 still fixes the class of bug and makes it visible.
3. "The eyes are not a stereo pair." Measured 32.7 mean-abs-diff when static, but 11.5
   after a head turn, which is normal parallax. Stereo works; the divergence is a
   symptom of the freeze, not the disease.

**Still broken: the freeze-then-rescale.** Reported again after all of the above. What
is ruled out: the resolution (it now stays 2750x2850), the adapter (matched), the FOV
(100), the mono/stereo UV race (fixed), and the hand drive (working). What is NOT ruled
out and is where to look next:

- The **shutdown crash is a teardown race** and may share a root with the freeze: three
  runs ended with `EXCEPTION 0xc0000005 writing 0xDEDEDEDE` inside `d3d11.dll`, two
  threads at once, immediately after `PreExit` stops the pace thread. 0xDEDEDEDE is
  freed-memory poison. The detached pace thread is touching released D3D11 objects.
- The pace thread owns every runtime call while the game thread owns capture and
  UpdateSubresource on the SAME `g_ctx11`; ID3D11DeviceContext is NOT thread-safe.
  `ID3D10Multithread` is enabled but that protects the device, not a stale pointer.
- Instrument the frame path next: log around the Reset/teardown boundary and around
  every `g_ctx11` use from the pace lane, and get a minidump analysed from
  `%LOCALAPPDATA%\DishonoredVR\dumps`.

### 2026-09-02 - session 1: development framework

Explored the 22,959-line `src/dllmain.cpp` and the BioShock trilogy mod; planned the refactor
with the user (decisions: CMake+MSVC; DXVK restored in-repo and kept as the stereo path; both
backends kept behind one pipeline; retired code to `src/legacy`; a proper logging/debugging
surface). Executed: DXVK restore (52 patch commits + the M8.4 revert; `fork-patches/` removed),
submodules and vendored OpenVR, CMake scaffold and MSVC port (naked stubs, `.def`,
`_ReturnAddress`, `ID3D10Multithread`), the unity split, Phase 2 utilities, harness copy and
adaptation, simulator build + selftest PASS, debug surface, patterns.h, legacy gating, backend
probe, docs. Found: `dxvk_vr_view` is resolved by the proxy but absent from the published
patches (the handoff confirms it is the unshipped p53 commit); the hand-skin `.mtl` path used
`\v` and `\%` escapes so materials never loaded (fixed); 165 em dashes swept. Received the
author's handoff (their build 39.4) at the end of the session: version renumbered to 40.0.0,
the 39.x fixes and the adapter hypothesis folded into ROADMAP, KNOWN_ISSUES, CODE_REVIEW,
ENGINE_NOTES and XR_HANDOFF. Verification: exports 9/9, lint clean, both legacy
configurations build, `split-source.py --check` reports only the intended changes. Branch
pushed.
