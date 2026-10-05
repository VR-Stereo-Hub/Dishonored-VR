# Plan: the grip as the interaction button, and a hand that shows it

Status: step 2 BUILT (2026-10-05, compiles, not yet run in the simulator or the headset); steps
3 onward not built. Written 2026-10-05 on `claude/fast-dlss-physical-pickup` (draft PR #181),
after four headset sessions of the first part. Read it top to bottom before touching code;
section 9 is the order of work, section 10 has the maintainer's answers.

## 1. What is asked

1. The grip works on more than loot and doors: things that can be carried and thrown, and the
   other interaction prompts. **Not** talking to someone.
2. Whenever a hand is eligible to pick something up or interact, the weapon or item model in
   THAT hand disappears and the hand takes the normal open pose, as the signal that a grip will
   act. It comes back when the hand is no longer eligible.
3. This must not act on takedowns (it is not expected to, and it must be shown not to).

## 2. What exists and is accepted (do not rebuild it)

Code: `src/game/dishonored/physical_pickup.cpp`, two lines in `interact_aim.cpp`
(`PickupRay`), `PickupPadFilter` in `core/input/pad_bridge.cpp`, the reading hand in
`core/gfx/hud_layout.cpp`. Notes: ENGINE_NOTES "Physical pickup", CONTROLLER_BINDS,
HUD_ANCHORS "The reading panel on either hand".

- An incremental GObjects sweep lists actors by class (name-index tests, at most 60 us a
  frame); a rotating pass keeps the ones near the camera; each frame the nearest one within
  reach of a hand becomes the TARGET; the VR-166 interaction bridges aim the engine's own trace
  at it; the engine focuses, highlights and prompts; while the engine's focused actor is the
  target, that hand's physical grip is swallowed before the bind remap and Interact is held
  130 ms.
- Kinds today: loot (`DisPickup_Base` and children, `DisProjectile_Arrow`), readable loot
  (`DisAbstractItemPickup`: own reach 45 cm, page on the opening hand), doors (`DisDoor`:
  measured from the collision box, own switch and reach).
- Headset state: loot, books, the page on the opening hand and doors are accepted. The module
  costs about 16 us a frame.

Two things were tried and removed, and must not come back without new evidence:

- **Stacked books** (commit 111b93de2, removed in the commit after it): taking the game's own
  focus as the target, leaving a grip-opened book out for 15 s, and twelve aim variants.
  Reported worse in the headset. The lower book is opened by pointing.
- **An extra 45 degree tilt for the right-hand page**: reported worse. The mirrored reference
  stands as it is.

## 3. The two rules this module has already paid for

1. **Bound the worst frame, not the mean** (TRAPS, 2026-10-05; FLICKER_REFERENCE top entry). A
   per-frame cost of 0.32 ms with occasional slow frames starved the right eye 17 times a
   minute under AFW. Anything added here states its cost from the module's own beat line
   (`own cost`, the over-250-us and over-1000-us counts) before a headset sees it, and those
   counts stay at zero. The `pickup/silent` line names the frames before every camera-silent
   draw.
2. **The engine decides.** No field is written and nothing is activated by the mod. Every
   refusal stays the engine's. The grip only presses Interact while the engine's focused actor
   is the target.

## 4. Which things the grip should reach

The game's own answer is "whatever implements `DisInteractableInterface`". From the script
declarations (class hierarchy only), the classes that declare it:

| Class (base) | What it is in play | Grip |
|---|---|---|
| `DisPickup_Base` | loot, keys, elixirs, bone charms, notes | done |
| `DishonoredUsableObject` | doors (done), levers, valves, wheels, cabinets, switches | yes |
| `DishonoredMovable` (and `DisWhaleOilBattery`) | things carried and thrown: bottles, tanks | yes |
| `DisGrenade`, `DisWhiskeyBottle`, `DisMovableLimb`, `DisDLC07SkeletalMovable` | loose throwables and physics props | yes |
| `DisProjectile` (`DisProjectile_Arrow` done) | bolts and darts to recover | yes |
| `DisGadget_SpringRazorPlaced`, `DisDLC06Gadget_ArcMinePlaced`, `DisTripwire` | placed traps (take back, disarm) | yes (maintainer, 2026-10-05) |
| `DisProjectileLauncher` | wall-mounted launchers, rewire points | yes |
| `DisRiverKrust`, `DisUpgrade` | pearl, upgrade pickups | yes |
| `DisClimbable` | chains | yes (maintainer, 2026-10-05) |
| `DishonoredNPCPawn` | talk, choke, pick up a body | **NO** |
| `DisDialogInanimateDummy`, `DisSpeaker_PA` | talk targets that are not pawns | **NO** |
| `DisGameCrowdAgentSkeletalRat`, `DisTrigger` | rats, volume triggers | no |

- "Not talking" and "not takedowns" are the same exclusion: no pawn is ever a target, and the
  two dialog classes are excluded by name. A choke and a takedown are pawn interactions, so
  the list never offers them; section 8 says how that is shown rather than assumed.
- Prefer ONE generic test over a growing name list: a class is a candidate if it or a parent
  declares the interface, minus the exclusions above. Statically that is the table; at run
  time the cheap equivalent is name indices for the ten base names (the existing
  `PpNameVerdict` already works that way). Do not walk `UClass::Interfaces` unless a name
  list proves wrong: its layout is not derived for this build.
- Each kind needs a distance rule. Small things: the actor origin (as loot). Large or
  off-centre things (usables, launchers, movables bigger than a bottle): the collision box,
  exactly as doors do (`Actor.CollisionComponent` -> `PrimitiveComponent.Bounds`). Start with
  the box for everything that is not `DisPickup_Base`; it degrades to the origin when the
  component is missing.
- One F10 switch per group is enough: loot (exists), doors (exists), "objects you can carry or
  throw", "levers, switches and other prompts". Reach: reuse the loot reach for small things
  and the door reach for box-measured things until the headset asks for more.

**Hold-to-use.** Some usables want Interact HELD (`DishonoredUsableObject` has a
`UsableObjectHoldToUse` group: `m_pRemoteActorToDrive`, `m_fInitialProgress`). Today the
filter presses Interact for a fixed 130 ms. Change it to: Interact is down for at least 130
ms and for as long as the swallowed grip stays down. A tap still taps; a held grip holds.
Check against the sheathe gesture: holding Interact with nothing focused sheathes the weapon
in this game, so the hold must end the moment the target is lost.

**Carried objects.** Picking up a `DishonoredMovable` puts the game in its carry state
(`throw_aim.cpp`, VR-181, owns where the carried object sits and how it is thrown). The grip
only starts the carry. While something is carried the module must not target anything (the
engine's own prompt is "throw/drop"), and the grip must go back to its bound action. The carry
state is already read by the mod: find the flag `CarryHoldTick` uses rather than deriving one.

## 5. Eligibility, per hand

Today the pad bridge reads one mask (`g_ppReadyMask`: bit h = hand h may take the target now)
with a freshness stamp. That mask is exactly "this hand is eligible". The new consumers are on
the render thread (the weapon draw, the hand pose), so publish it the same way: atomics, with
the timestamp, read with a short hold so one missed frame does not flash the weapon back.

Decide and write down, before building, WHICH moment counts as eligible:

- (a) the hand is within reach of a target (the trace is aimed, the engine has not answered), or
- (b) the engine has focused the target (the highlight is up and a grip will act).

(b) is the honest signal ("a grip will act now") and is the existing mask. (a) appears a few
frames earlier and also shows when the engine refuses. Build (b). Add hysteresis in time, not
in distance: eligible after 2 consecutive ready frames, not eligible after 150 ms without one.
A weapon that blinks at the edge of reach is this feature's version of the flicker.

## 6. Hiding what the hand holds

What is known:

- Held weapons and items are drawn by the game and placed by `hands/weapon_attach.cpp`
  ("wa"): a contract per asset and HAND (`k->hand`, `w->hand`), found by draw identity. `WaDraw`
  already drops draws it must not show (`wa: SUPPRESSED a weapon draw this build did not
  place`, `wa/id: DROPPING an uncorrectable pass`). So "do not draw this hand's contract" has a
  natural place and an existing vocabulary.
- VR-33-HANDS-AND-WEAPONS.md section 8 is the graveyard of every way a weapon draw was
  mis-identified. Read it first. The dark copy at the native position (an uncorrected pass of
  the same geometry) is the failure to expect: hiding the corrected pass and not its siblings
  shows exactly that copy.
- The left hand's item (crossbow, pistol, a power's hand effect) and the right hand's sword go
  through the same module but not always the same path; `aim_item_kind(hand)`
  (`bolt_model_ray.cpp`) and the primary/secondary kind reads (`g_rflPrimaryKind`,
  `g_rflSecondaryKind`) say what each hand holds.
- Hand effects attached to the hand (`hands/fx_follow.cpp`: a power's glow) are separate draws
  and will stay visible unless hidden too. Decide per effect; the first build may leave them.

To derive before writing the hide:

1. Every pass that draws the held item for one hand in one frame (corrected pass, ghost
   passes, shadow or depth passes, the AFW foreground mask pass). `wa:` beat lines count
   "placed / refused / ghost passes" per contract: start there.
2. Whether dropping all of a contract's passes for one hand leaves anything behind (a shadow,
   a mask hole in AFW's foreground, DLSS's hand mask). AFW rebuilds the held eye's hands from a
   foreground mask (`afw/warp`, `fgmask`); a weapon that vanishes from the colour pass and not
   the mask leaves a weapon-shaped hole.
3. The engine alternative: is there a holster or hide that the game itself does for the held
   item (the sheathe on a held Interact is one; cinematics hide weapons too)? An engine-side
   hide lets attachments follow for free (the project rule), but changes game state. Prefer the
   draw-side drop unless it cannot be made clean; record why.

Design: one function, `bool WaHandHidden(int hand)`, read at the top of the contract's draw
path; when true, every pass of that hand's contracts is dropped and counted
(`wa: hidden for interaction, hand H, N draws`). Default off behind
`[Aim] PhysicalHideWeapon`, live toggle, and it must fail open: any doubt about identity
draws the weapon.

## 7. Opening the hand

What is known:

- `OhActive()` in `hands/mesh_split.cpp` is the gate for the right hand's open pose: today it
  opens when the right hand is EMPTY (`[Hands] OpenEmptyRightHand`), by mirroring the left
  hand's finger bones onto the right (`g_ohPair`, "hands/openright"). It already refuses while
  a game animation owns the hand (`dvr::anim::hand_owned(1)`: takedown, choke, cinematic).
  That refusal is the takedown guarantee for the pose; keep it first in the test.
- So the right hand's open pose exists and needs one more reason to be active: "this hand is
  eligible and its weapon is hidden".
- The LEFT hand has no such path: its open pose is the game's own when it holds nothing, and
  the source of the mirror. When the left hand holds a crossbow or a power, its fingers are
  posed by the game around that item. Opening it needs a stored open pose to write (the left
  hand's own empty pose, captured when it is empty) or the right hand's mirrored the other
  way. Capturing the left hand's empty pose once and replaying it is the smaller step.

To derive: where the left hand's finger bones can be written each frame without fighting the
game's animation (the same palette route the right hand uses), and what `hand_owned(0)` says
during left-hand actions.

Order matters: hide the weapon and open the hand in the SAME frame, both from the one
eligibility flag, or the hand shows a fist with no sword, or an open hand through a sword.

## 8. What must be shown, not assumed

- **Cost.** `own cost` in the beat line stays in the tens of microseconds with the wider
  class list (more listed actors: the near list bounds it; say the numbers).
- **No pawn is ever targeted.** Log the class of every target (it does) and count targets
  whose class chain contains a pawn name: the count must be printed and read zero after a
  session that includes standing next to NPCs. An instrument that cannot print the unwelcome
  answer is not evidence.
- **Takedowns and chokes are untouched.** With a hand at an NPC's neck: no `pickup: target`
  line, the right grip still chokes, the weapon is not hidden.
- **Combat.** A grip pressed to block within reach of an interactable becomes an interaction.
  That was accepted for loot and doors; with every prop in the level eligible it will happen
  far more often, and the weapon will also vanish from the hand in a fight. Offer a guard and
  let the headset choose: no eligibility while the weapon was swung or the trigger pulled in
  the last second, or none while enemies are alerted (find an existing combat/alert read; do
  not invent one).
- **Flicker.** Any report of flicker starts at FLICKER_REFERENCE section 1. The rows to expect:
  the dark weapon copy at the native position, a weapon-shaped hole in AFW's rebuilt eye, the
  hand pose popping at the edge of reach. Record every result there in the same commit.
- **The simulator first.** Class lists, target lines, grip swallowing, the hidden-draw counts
  and the cost are not perceptual: `tools\xrsim-launch.ps1` answers them. Ask before every
  launch. What the open hand and the missing weapon LOOK like is for the headset.

## 9. Order of work (one behavioural change per build)

1. Ticket and branch. No Linear ticket exists for #181 (no Linear access in the sessions that
   built it); find or create one. Decide with the maintainer whether this continues on
   `claude/fast-dlss-physical-pickup` or starts a branch on top of it.
2. **Wider targets, no visuals.** The class table of section 4 as kinds with a box or origin
   rule, the two new F10 switches, the pawn counter, hold-to-use, the carry-state gate. Sim,
   then headset. Accept before going on.
3. **Eligibility published per hand** with the time hysteresis, and a log line on change. No
   consumer yet. Sim.
4. **Hide the held item** for an eligible hand (section 6), right hand first (the sword: one
   asset, the best understood contract). Default off, live toggle. Sim for the draw counts,
   headset for the look under reentry AND AFW, with and without DLSS.
5. **Open the right hand** while its weapon is hidden (section 7, the existing mirror).
6. **The left hand:** hide, then open (the captured empty pose).
7. Combat guard, if the headset asks for it.
8. Docs in each commit: ENGINE_NOTES (classes, offsets, what a carry does), VR-33 notes (the
   hide and what it had to drop), FLICKER_REFERENCE (every flicker result), CONTROLLER_BINDS,
   STATUS.

## 10. Open questions for the maintainer - ANSWERED 2026-10-05

- Chains (`DisClimbable`): **the grip grabs chains.** In the usable group.
- Should the hand open for doors too? **Yes: the hand opens for doors** (step 5 and 6 treat a
  door like any other target).
- Placed traps: **the grip takes traps too**, in the usable group. Everything the grip reaches
  keeps working with the Interact button exactly as before.
- While a body is carried: **the free hand can still interact.** Built as: the hand whose
  animation lane holds the body is not offered a target; the other is (section 4, ENGINE_NOTES
  "The carry states, per lane").
- Combat guard (section 8): **none for now.** Step 7 stays unbuilt unless the headset asks.

## 10b. What step 2 built (2026-10-05)

- The class table of section 4 as five kinds (loot, book, door, carry, usable) plus an
  excluded set that ends the class walk with a no (pawns, talk targets, rats, triggers). Carry
  and usable are box-measured with the door reach; F10 switches "Pick up things you can carry
  or throw by grabbing" (`[Aim] PhysicalCarry`, 1) and "Use levers, switches, chains and traps
  by grabbing" (`[Aim] PhysicalUsables`, 1); seam `pickup carry|usables on|off`.
- The pawn counter: each new target's class chain is read again as text; a name containing
  `Pawn` (or a talk class) is refused, blocked for a minute, and counted in the beat line.
- Hold-to-use, for usables only: loot, books and doors keep the accepted 130 ms tap.
- The carry gate: nothing targeted while a movable is carried; while a body is carried only
  the free lane's hand.
- What the first run must show (simulator first): the beat line's per-kind list counts and
  `own cost` with its over-250/over-1000 counts at zero; `pickup: target ... [carry]` and
  `[usable]` lines; `pawn or talk targets refused 0` after standing next to NPCs; a
  `pickup: carry gate` line when a bottle and when a body is picked up; a usable's hold line.

## 11. Tools to use before deriving anything

- `tools/uscript/dishonored/` (local): the class hierarchy and property names above came from
  it. `DisTweaks_*` files hold the interaction distances.
- `FindPropOffset` / `RflOffsetOf` / `FindBoolProp`: every offset the module uses is resolved
  by name; keep it that way.
- `propwatch on`: if a state has no name yet (the carry flag, an alert state).
- `tools\ida-run.ps1` with the staged database: only if an engine-side hide is pursued.
- The log: `pickup:` lines, `wa:` lines, `hands/openright:`, `pe/cost`, `reentry: gates`.
