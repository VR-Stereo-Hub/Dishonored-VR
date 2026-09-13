# VR-88 plan: know when a scripted animation owns the body, and hand it back

**Status: phases 1 and 2 implemented together, enabled by default at user request. Headset validation pending: the first playtest after implementation ran the previous installed build, so it is not evidence for this code.** Branch `claude/vr-88-anim-handback`, off
`VR-Main` at `5e076813`. Ticket VR-88; the long-term layer is VR-89.
Sections 2 and 3 are the research and the claims a run must prove; sections 4 to 6 are the design, and the implementation notes at the end record what landed.

## 1. The problem and the goal

During a takedown, a choke, a drop assassination or a fatality, the game animates the
arms and the held weapon through the move. The mod keeps doing its own thing on every
draw: the bone palette correction moves the hands and the weapon to the controllers,
the arm split draws only the hand triangles, and the weapon attachment suppresses the
copies it did not place. So the move plays with the player's hands wherever the
controllers happen to be, or with nothing visible at all.

**Goal of VR-88, in two phases:**

1. **A flag.** Read, not infer, whether a scripted full-body action owns the player's
   body right now, and which one. Read-only, log changes, publish a snapshot.
2. **The hand-back.** While the flag says the game owns the body, stop the hand and
   weapon overrides and let the game's own arms and weapon draw; take them back
   afterwards without a pop. Behind a default-on lever with a live A/B.

**The long-term goal (VR-89)** is an animation control layer: know exactly which
animation plays, allow or suppress specific ones, trigger animations on demand, and pose
the hands freely. Section 8 sketches how the pieces found here serve that, so phase 1 is
built as its first layer and not as a one-off.

## 2. What the decompiled scripts say

Declarations only; the dump has no function bodies. Names are the claims; **every offset
is derived at runtime by name**, never taken from here. Reviewer: please spot-check the
class and property names against `tools/uscript/dishonored/`.

### 2.1 The player runs three native state machines, and the master one is the flag

`DishonoredPlayerPawn` declares three `DishonoredNativeStateMachine` members:

| Member | Role, from its default object |
|---|---|
| `m_pPlayerMasterFSM` | the whole body: 23 state templates (below) and a transition table |
| `m_pPlayerUpperFSM` | a `DisNativeStateMachine_PlayerAction`, `m_ActionUsage = Upperbody`, bound to the `ANIMSTATE_UPPER_BODY` picker |
| `m_pPlayerLeftArmFSM` | a `DisNativeStateMachine_PlayerAction`, `m_ActionUsage = LeftHand`, bound to the `ANIMSTATE_SPECIAL` picker |

`DishonoredNativeStateMachine` declares `m_pCurrentState` (a `DishonoredNativeState`
object), `m_pCurrentStateID` (a `Class`), `m_pPendingStateID`, `m_pPendingState`,
`m_bIsLocked`, the template array `m_NativeStates`, and `m_pTransitionLogic`.

**So "what is the body doing" is the CLASS NAME of the master machine's current state.**
One pointer chain and one class-name fetch: pawn, `m_pPlayerMasterFSM`,
`m_pCurrentState`, `ObjClassName`. `m_pCurrentStateID` is the same answer by another
route and is the cross-check.

Master FSM states (template classes): `StatePlayerMasterWalk`, `Leaning`, `Swim`,
`Jump`, `Falling`, `MasterAction`, `Versus`, `ChangeReadyStance`, `Mantle`, `Stunned`,
`Dead`, `InDialog`, `Soiree`, `InScriptedChoice`, `Assassinate`, `HolePeeking`, `Climb`,
`PrePossess`, `Possess`, `Slide`, `Minigame`, `Choke`, `InStore` (every class is
`StatePlayerMaster<name>`; a few template OBJECTS are named `StatePlayerWalk_Template`
and similar, which does not matter because the reader uses the class).

Upper FSM states: `StatePlayerUpperIdle`, `StatePlayerMeleeAttack`, `StatePlayerBlock`,
`StatePlayerGenericFatality`, `StatePlayerAction`, `StatePlayerTransitionItemIn`,
`StatePlayerEquipChange`, `StatePlayerChangeReadyStance`, `StatePlayerGrabMovable`,
`StatePlayerGrabCorpse`, `StatePlayerCarryCorpseIdle`.

Left arm FSM states: `StatePlayerUpperIdle`, `StatePlayerAction`,
`StatePlayerTransitionItemIn`, `StatePlayerEquipChange`, `StatePlayerUpperNav`,
`StatePlayerGrabMovable`.

Supporting evidence that `Assassinate` and `Choke` are exclusive, long-lived states: the
master transition table marks leaving either for any `DishonoredNativeState` as
NotAllowed, with explicit exceptions only to `Dead`, `Swim`, `Soiree` and
`Choice_Base`. The table gives the same locked-in shape (leave for anything: NotAllowed,
then a short allow list) to `Versus`, `Minigame`, `Stunned`, `Soiree`,
`InScriptedChoice`, `InStore` and `InDialog`: nine states in all where the engine itself
refuses to let ordinary movement interrupt. That list is the natural first draft of
"the game owns the body", before any run. `StatePlayerMasterAssassinate` also defaults `m_bAllowIncomingAttacks`
to false. `StatePlayerMasterChoke` and `StatePlayerMasterMantle` both derive from
`StatePlayerMasterAction`.

### 2.2 The earlier candidate is a config value, not a live flag

`GAMEPLAY_STATE.md` and ENGINE_NOTES name `eDisPlayerActionUsage_Fullbody` as "the
takedown and choke discriminator". The dump does not support reading it that way:
`eDisPlayerActionUsage` is the TYPE of `DisNativeStateMachine_PlayerAction.m_ActionUsage`,
set once per machine in its default object (Upperbody, LeftHand). Nothing declared holds a
changing Fullbody value. **Those two docs should be corrected in the same commit as the
code**, with this plan as the reference. If the reviewer finds a live holder, this
section is wrong and says so.

### 2.3 Secondary evidence, readable by name

| Where | What it adds |
|---|---|
| `DishonoredPlayerPawn.m_BodyMode` (`ePlayerBodyMode`: `ARMS_ONLY`, `FULL_BODY`, `HIDDEN`) | whether the game has switched to the full-body mesh. ENGINE_NOTES: it swaps meshes on the one component. A likely co-signal for takedowns; to be MEASURED, not assumed |
| `DishonoredPlayerPawn.m_pAnimStateComp` (`DisAnimStateComponent`) | `m_StatePickers` (one per picker: current state pointer, `m_bLockedOut`, `m_bLockedOut_Children`), and **`m_AnimSeqHistory[12]`**: a ring of (sequence FName, picker index) with `m_iCurNewestSeqInHistory`. The names of the last twelve sequences played, per picker. This is "which animation", readable with no hook |
| `DishonoredPlayerPawn.m_AnimStates_FullBody_Navigation / _Assassination / _Fatality / _Misc`, `m_AnimStates_UpperBody_Items(_Melee/_Ranged) / _Misc`, `m_AnimStates_LeftHand_Items`, `m_AnimStates_Special_Misc` | arrays of `DisPawnAnimState` (state name, blend time, tree name, non-looping, custom sequences, root motion mode). The vocabulary the history's names come from |
| `DisItemContext.m_ContextStatus` (`Idle / Failed / InProgress / Finished`) | per weapon action; `DisItemContext_Choke.m_State` (`In / Loop / Win / Win_HoistCorpse / Lose / Cancel`), `DisItemContext_Fatality.m_CachedFatalityType` (`Synced / Generic`) |
| `DishonoredPlayerPawn.m_pMatineeBlender` (`ArkAnimNodeBlendPose`) | the cinematic pose blend; `m_bEnabled`, `ActiveChildIndex` |
| `DishonoredPlayerPawn.m_pLookAtControl_LeftHand / _RightHand / _Camera` | `SkelControlSingleBone`s already on the rig (VR-30 notes the camera one) |
| `DisTweaks_Assassinate` move sets (front, back, left, right, fast, bend-time-frozen, special) and `DisItemAction.m_AnimStates_NonReady` | the tuning names a takedown picks its animation from |

### 2.4 The callable surface, from the native registration table

`python tools/ue3-natives.py --verify <exe> natives --grep <term>` (the crossbow
re-derivation passed before answering). Registered execs relevant here:

* `AnimNodeSlot`: `PlayCustomAnim`, `PlayCustomAnimByDuration`, `StopCustomAnim`,
  `SetCustomAnim`, `GetCustomAnimNodeSeq`; `AnimNodePlayCustomAnim`: the same three.
* `SkeletalMeshComponent`: `FindAnimNode`, `FindAnimSequence`, `FindSkelControl`,
  `UpdateAnimations`, `SetAnimTreeTemplate`.
* `AnimNode`: `PlayAnim`, `StopAnim`, `ReplayAnim`, `FindAnimNode`;
  `AnimNodeBlend` / `AnimNodeBlendPerBone`: `SetBlendTarget`;
  `ArkAnimNodeBlendPose`: `SetActiveChild`.
* `SkelControlBase`: `SetSkelControlStrength`, `SetSkelControlActive`;
  `SkelControlLookAt`: `SetTargetLocation`, `SetLookAtAlpha`.
* Cheats, useful as TEST instruments: `PlayerSetFixedFatality`, `PlayerDisableFatality`,
  `PlayerDisableAssassinate`, `TogglePlayerBodyMode_Native`, `NPCForceFatality`.
* `DishonoredPlayerPawn.OnToggleChoke` (a Kismet action handler).

**Negative, which matters for VR-89:** no exec touches the player state machines. Their
transitions are native only, so from outside they are READ-ONLY; suppressing a state is a
native-hook problem, not a function call.

The call-by-name lane already exists: `FindFunctionObj` plus a direct `ProcessEvent`
call, used by `console.cpp` and `mat_hide.cpp`. `FindFunctionObj` matches the function
NAME only, and `PlayCustomAnim` exists on two classes, so any use of it must also match
the function's Outer.

## 3. Claims the first run must prove or kill

Rule 3 of GAMEPLAY_STATE: a flag is not a flag until a run shows it CHANGING.

| Claim | Prediction | Killed by |
|---|---|---|
| C1: the master state names the action | walk, jump, mantle, slide, choke, takedown each show their own `StatePlayerMaster*` class, entering at the move's start and leaving at its end | a takedown that leaves the master state at `Walk`, or a state that never changes |
| C2: front/back takedowns and drop assassinations are all `Assassinate` | one class for all three | a different class per variant (then the classifier grows, the design holds) |
| C3: fatalities live in the UPPER machine | `StatePlayerGenericFatality` in upper while master stays in a combat-capable state | a fatality with no upper change |
| C4: `m_BodyMode` moves with full-body moves | `FULL_BODY` during takedowns and chokes, `ARMS_ONLY` otherwise | no change in any move (then it is only for cinematics, and it drops out of the classifier) |
| C5: the sequence history identifies the animation | a new FName at the start of each move, different per move | a history that never advances |
| C6: the state pointer is stable within a state | the same `m_pCurrentState` pointer for the whole move | a pointer that changes every tick (templates cloned per entry would still work, but freshness rules change) |

## 4. Phase 1: the flag (read-only)

### 4.1 Where and how it reads

New module `game/dishonored/anim_state.cpp`, script lane, ticked from the ProcessEvent
camera pass beside `RflTick`.

* **Resolve by name, once, lazily**, through `ue3/reflect.cpp`'s cached `RflOffsetOf`,
  behind its GNames gate: `DishonoredPlayerPawn.m_pPlayerMasterFSM / m_pPlayerUpperFSM /
  m_pPlayerLeftArmFSM / m_BodyMode / m_pAnimStateComp`,
  `DishonoredNativeStateMachine.m_pCurrentState / m_pCurrentStateID / m_pPendingStateID`,
  `DisAnimStateComponent.m_AnimSeqHistory / m_iCurNewestSeqInHistory`. Every miss logs the
  class and property and leaves the flag UNKNOWN.
* **The pawn** comes from the existing controller-to-pawn path (`crouch.cpp` already
  resolves it). `IsLiveObject` gates the pawn when its pointer changes, and each FSM
  object when its pointer changes. These are reads, but a stale pawn after a level load
  is exactly the case the class-name guard failed on in VR-85, so the stronger test is
  used from the start.
* **Per tick** it reads three pointers and compares them with the last ones. Only a
  CHANGED state pointer pays for `ObjClassName`; the name is cached against the pointer.
  The sequence history is read as one index plus one FName when the index moves.
  Cost per tick: a handful of dword reads. No per-property validation (TRAPS, the
  PropWatch frame-budget entry).
* **No writes of any kind.**

### 4.2 What it publishes

A snapshot under an SRW lock, with a generation and a script-lane timestamp:

```
master, upper, left   state class name (interned), state pointer, entered-at ms
pending master        class name or none
bodyMode              0/1/2 or unknown
lastSeq               newest history FName and its picker, and when it changed
owner                 PLAYER | GAME | UNKNOWN, and the rule that decided it
```

Consumers on the present thread take a copy and check the generation's age; a snapshot
older than 150 ms reads as UNKNOWN (fail inert: today's behaviour).

### 4.3 The classifier

Data, not code: `[Anim] HandBackMaster=` and `[Anim] HandBackUpper=`, comma lists of
class names, with a compiled default that is a HYPOTHESIS to be confirmed by the phase 1
run:

* master: `StatePlayerMasterAssassinate, StatePlayerMasterChoke, StatePlayerMasterMantle,
  StatePlayerMasterClimb, StatePlayerMasterStunned, StatePlayerMasterDead,
  StatePlayerMasterPrePossess, StatePlayerMasterPossess, StatePlayerMasterMinigame`
* upper: `StatePlayerGenericFatality, StatePlayerGrabCorpse`

Open for the run to decide, deliberately NOT in the default: `Slide`, `Versus`,
`InDialog`, `Soiree`, `InScriptedChoice`, `InStore`, `HolePeeking`, `CarryCorpseIdle`,
`MeleeAttack`.

Owner = GAME when any listed state is current. **Hysteresis:** GAME is entered
immediately; PLAYER returns only after the machine has been out of every listed state for
`[Anim] ReleaseMs` (default 250, not measured), so a state that flickers through `Walk`
between two scripted states does not bounce the hands.

### 4.4 Instruments

* The log, changes only:
  `anim: master StatePlayerMasterWalk -> StatePlayerMasterAssassinate (pending none) | upper StatePlayerUpperIdle | left StatePlayerUpperIdle | body ARMS_ONLY -> FULL_BODY | seq <name> (picker 0) | owner PLAYER -> GAME by master rule`.
* A 5 s beat that prints even when nothing changed, with the resolve status and the
  snapshot age, so "never resolved" and "nothing happened" read differently.
* `anim status`, the F10 Debug line, and `status.json`.
* `[Anim] StateWatch` gates the whole reader: **ships 1 (user-requested default)**, armed in the tester's
  installed ini for the run.

### 4.5 Host tests

A pure classifier with injected snapshots: entry is immediate, release honours ReleaseMs,
a stale snapshot reads UNKNOWN, an unresolved property reads UNKNOWN and never GAME, the
ini lists parse with spaces and unknown names, and a name not in any list is PLAYER.

## 5. Phase 2: the hand-back

Built together with phase 1 at the user's request; C1 remains to be checked in the combined headset run. One lever, `[Anim] HandBack`, **ships 1 (user-requested default)**,
live `anim handback on|off`, an F10 Hands checkbox, effective value logged with its source.

### 5.1 What stops while the owner is GAME

Named by module, so the reviewer can check nothing is missed:

| Override | Module | While GAME |
|---|---|---|
| hand placement (the palette correction D on the hand draws) | `mesh_split.cpp` `MsDraw` | D blends to identity (5.2), then the draw passes through |
| arm suppression (drawing only the hand classes) | `mesh_split.cpp` `MsDraw` | the full player mesh draws: the game's own arms |
| our D3D11 hands over the image (VR-31) | `core` `HandDrawFn` from `present_tick.cpp` | not drawn |
| weapon placement and duplicate suppression | `weapon_attach.cpp` `WaDraw` | pass through, suppress nothing |
| SkelControl rotation drives | `skelcontrol.cpp` `SkcRotApply` | not applied |
| arm hiding | `arms_hide.cpp` | not applied |
| the aim laser and dot (VR-57) | `aim_visual` | hidden |

Unchanged: head tracking, the camera seam, the neck term, fire seams (the game does not
fire during these moves; logged if it does).

### 5.2 No pop in either direction

The palette route already composes one rigid D per draw. On entry D slerps from its last
target to identity over `[Anim] HandBackBlendMs` (default 150, not measured), and only
then do the arm split and weapon suppression release; on exit the split and suppression
resume at identity and D slerps back to the controller target. The arm cut cannot blend,
so it switches at the identity end of each blend, where the hands are exactly where the
game draws them.

### 5.3 Lanes

The owner is read on the script lane; draws happen on the render thread up to a frame
later (UE3's one-frame thread lag, the reentry ring's comment). A handback that lands one
frame late shows one frame of our hands at the start of a move, which the blend hides.
Reviewer: is a per-draw generation match worth the complexity here, or is the blend
enough?

### 5.4 Fail inert

Unresolved, stale, lever off, or pawn not live: the owner is PLAYER and every module
behaves exactly as today. A module that cannot identify identity for its D refuses the
blend and passes the draw through untouched, logging why.

## 6. Launches

**Launch 1 (phase 1 build, `StateWatch=1`): which states do these moves enter?**
Load a save with a guard nearby. In order, with a few seconds of normal walking between
each: jump; mantle onto a ledge; sprint and slide; choke a guard from behind; a stealth
takedown from behind; a takedown from the front (or a drop assassination if easier); a
combat fatality; pick up and drop a body. Quit through the menu.
Answers C1 to C6 from the log. Outcomes: C1 holds, go to phase 2 with the measured lists;
C1 fails, the reader is wrong or the flag lives elsewhere, and the next step is the
sequence history (C5) or `propwatch` on the pawn during a takedown.

**Launch 2 (phase 2 build, `HandBack=1`): do takedowns show the game's arms, and do the
hands come back cleanly?** The same moves; judge the start and end of each. F10 toggles
the lever for an A/B inside the run.

## 7. Rules carried

No hardcoded offsets; property names resolved and logged. `IsLiveObject` for any object
whose pointer is retained across ticks. Log changes, not state. The VR-88 levers default on at user request,
with a live A/B. The installed ini is diffed in full on every install. No game content
committed: this document lists names only.

## 8. Toward VR-89, the animation control layer

What phase 1 builds is layer one; each later layer is its own ticket.

1. **Which animation.** The sequence history and item-context states, joined to the FSM
   state, give (state, sequence name, context phase). That tuple is the key every later
   rule is written against.
2. **Rules per animation.** Allow, hand back, or suppress, keyed on that tuple, loaded
   from data.
3. **Trigger.** `SkeletalMeshComponent.FindAnimNode(slotName)` on the player mesh, then
   `AnimNodeSlot.PlayCustomAnim(seq, rate, blendIn, blendOut, loop, override)` through the
   existing ProcessEvent lane, the parameter frame resolved from the UFunction's own
   properties by name, the function matched by Outer. First target: a harmless idle
   variant, proved by the sequence history showing it.
4. **Suppress.** No script surface exists for the state machines. Candidates, cheapest
   first: neutralise at the node (slot weights, `SetBlendTarget` on the per-bone filters
   the pickers hold); find the native transition request with the natives table and a
   caller census (`tools/pe-xref.ps1`) and gate it. The transition table in 2.1 is the
   engine's own allow list and a natural place for such a gate.
5. **Pose.** Per-bone local rotations composed into the bone palette the hands already
   rewrite: no engine writes, and the bone map comes from the arm split's bone analysis.
   `SkelControlBase.SetSkelControlStrength` on the rig's own controls is the engine-side
   alternative, with the 32.6 freed-AnimTree hazard and `IsLiveObject` on every write.

## 9. Open questions for the reviewer

1. Is the master FSM's `m_pCurrentState` class name the right primary signal, or should
   `m_pCurrentStateID` (a Class, no instance) be primary because it cannot dangle?
2. Section 2.2 contradicts two existing docs. Agree, or is there a live Fullbody holder?
3. The default hand-back lists in 4.3: anything that must be in or out before the run?
4. Section 5.3: generation-matched hand-back per draw, or blend only?
5. Should phase 1 also read the item contexts (choke `m_State`, context status), or wait
   for layer 1 of VR-89?
6. Mantle and climb animate the arms too. Handing them back loses the controllers during
   every ledge grab. Include them by default, or leave them to a separate lever?


## Implementation notes for the combined build

`anim_state.cpp` samples the three FSMs on the script lane at most once per
10 ms, using the controller's reflected Pawn link. The state instance and class
ID must agree. Required read failures, watch off, or a snapshot older than 150 ms
mean UNKNOWN and retain the existing controller behavior. Optional body/history
failures do not invalidate an otherwise readable ownership signal. Pending state
and sequence names are resolved on changes; a five-second heartbeat exposes an
unresolved reader. The SRW-locked snapshot and status.json report state names,
owner, sequence, body mode and controller blend weight.

`StateWatch=1` and `HandBack=1` are both the compiled fallback and generated ini
default. Commands: `anim status`, `anim watch on|off`, `anim handback on|off`.
F10 Hands exposes the handback checkbox and current state. Master/upper comma
lists remain configurable through `HandBackMaster` and `HandBackUpper`; mantle
and climb are included. VR-89 suppression and on-demand playback remain deferred.

The correction uses shortest-path rotation interpolation, with uniform scale
and translation interpolated to identity. It is blended ONCE in the hand path
before publication to weapons. Weapon copies inherit that same correction
through their existing coordinate transforms. Re-blending in each weapon's
local frame is incorrect. A present consumes a fixed blend weight. Animated
source and controller target continue updating during the blend. This is
present-level consistency, not a claim of exact FSM/draw pairing.

At identity the draw router bypasses split and duplicate suppression, including
non-indexed draws. D3D11 hands and aim visuals are hidden during ownership and
blend-back. The script path releases hand SkelControl application flags and model
scale, detaches active graft donors, restores mesh rotation and mod-hidden arms,
and suspends further hand writes. Camera look-at and fire aiming remain
independent. Controller writes resume on return and the palette blends back.
Stale snapshots and a disabled lever return to existing behavior.

The optional sequence record validates as FName plus picker int, with stride
derived from the reflected picker offset. The property resolver gained a checked
form so a real member at offset zero is distinguishable from a lookup failure.
Property scans are initialization work, not repeated sample work.

One combined headset run remains: walking, jump, mantle/climb, slide, choke,
back/front/drop assassination, fatality, and pick up/drop a body. Compare
HandBack on/off in F10, check both eyes and action entry/exit, then reload a save.
The state labels and 150/250 ms blend/release settings remain hypotheses until
that run. A successful build does not confirm mappings or visual comfort.
