# Gameplay state: the flags the mod needs, and how to read them

**Status: the RESOLVER exists and one flag is read; the rest is wanted.** The
technique in section 5 has been in this repo since 38.x and is load-bearing in
four modules. `ue3/reflect.cpp` (VR-61) adds a cache and a TArray reader on top,
and uses them to read the equipped item per hand.

Still wanted: `EItemSocket` (equipped versus holstered), the per-hand stance,
`eDisPlayerActionUsage_Fullbody`, and a published snapshot with per-flag
freshness. Section 2 is the full list and section 6 is the rules.

Where a number appears here it is a starting hypothesis to be derived against the
running build, never a value to copy.

---

## 1. Why this exists

Almost every hard bug in this mod so far has been the same shape: **the mod acted
on a guess about game state because it had no reliable way to ask.** The cost is
recorded in three places and it is not small.

| What we guessed | What it cost |
|---|---|
| A draw on a weapon's buffers is that weapon | a fired bolt followed the player's hand for four builds (VR-59) |
| A component in the snapshot is a component in the player's hand | a whole gate that could not fire, because the snapshot is the pawn's INVENTORY (VR-59 attempt 1) |
| An asset name identifies an object | a loaded bolt and a fired bolt share the asset `bolt_01` and differ in everything else |
| The arms should always be hidden | takedowns and chokes play full-body animations that the player cannot see |

Each of those was answered eventually by reading the engine. The pattern is
clear enough to build for: **reading beats inferring, and a flag we can read is
worth more than a heuristic we have to tune.**

## 2. What the mod needs to know

Ordered by how much is currently blocked on it.

### Equipment and hands

| Question | Why the mod needs it |
|---|---|
| Which item is EQUIPPED, per hand | the weapon attachment must place the held item and leave every other instance of that mesh alone. Today it infers this from geometry |
| Which item is HOLSTERED | a holstered weapon is on the body, not the hand, and must not be corrected |
| Which hand an item belongs to | the sword/crossbow sides are currently an ASSUMPTION written into `WaHandFor`, not measured attachment data |
| Whether a projectile is loaded or in flight | the loaded bolt follows the hand, a fired one must not |
| Ammunition and counts | a reticle or a hand-mounted readout would need it |

### Player action and animation authority

| Question | Why the mod needs it |
|---|---|
| Is a full-body scripted action playing (takedown, choke, mantle, drop) | **the arms should be unhidden and animation control handed back to the game.** A takedown the player cannot see is a takedown that reads as a bug |
| Is the player blocking, or ready, or not ready | melee behaviour and hand pose |
| Is a cinematic or conversation running | the camera seam and the HUD both care |
| Is the player possessing, leaning, choking | each already has a documented yaw window (see ENGINE_NOTES); the mod should not fight them |

### World and camera

| Question | Why the mod needs it |
|---|---|
| Is the pause menu or an inventory screen open | the runtime layer loses its session on the pause menu (VR-51) |
| Is a Blink in progress, and to where | aim and comfort |
| Is the player in water, falling, on a ladder | comfort options |

## 3. What the engine already exposes

Read from the decompiled scripts. **These are class and member names only.** No
offsets, because offsets must be derived against the running build.

### The enums are the flags, and they already exist

```
EDisEquipUsage      None | Primary | Secondary
EItemSocket         None | Equipped | Holstered | ...
eDisPlayerStance    NotSet | NotReady | Ready | Blocking
eDisPlayerActionUsage   Fullbody | Upperbody | LeftHand
```

`EDisEquipUsage` is the per-hand channel. `EItemSocket` is the distinction
between equipped and holstered that VR-59's attempt 1 needed and could not get.
**`eDisPlayerActionUsage_Fullbody` is the takedown and choke discriminator** the
arm-unhiding work will be built on.

### Where equipment lives

```
DishonoredPlayerPawn
  m_pInventory : DishonoredInventory            (class name contains "Inventory")
  m_PlayerStance[EDisEquipUsage]  : eDisPlayerStance
  m_DesiredPlayerStance[EDisEquipUsage]

DishonoredInventory extends Object
  m_Slots      : array<PawnInventorySlot>
  m_AmmoInfo   : array<DisAmmoInfo>
  m_AbstractItem : array<DisAbstractItemInfo>
  m_pOwner     : DishonoredPawn

struct PawnInventorySlot
  m_pItem         : DishonoredInventoryItem
  m_pRequiredType : Class<DishonoredInventoryItem>
  m_RequiredUsage : EDisEquipUsage
```

**A slot carries the item AND the hand it belongs to.** That is the whole
equipment question in one 12-byte struct, and there is an array of them.

### Held items are components of the WEAPON, not of the pawn

```
DishonoredWeapon_Ranged
  m_pMesh        (pMesh)         : DishonoredItemSkeletalComponent
  m_pPlayerMesh  (pPlayerMesh)   : DishonoredItemSkeletalComponent

DisWepCrossbow        m_pArrowMesh_HighRes (pArrowMesh_HighRes)
DishonoredWepPistol   m_pBulletMesh        (pBulletMesh)
```

This is why a weapon's components persist when it is stowed: they belong to an
inventory item that still exists. **Presence is not equipment**, and `EItemSocket`
is the field that says which.

## 4. Why the current component walk cannot answer any of this

`FpCollect` in `hands/fp_mesh.cpp` breadth-first walks out from the player pawn,
reading every 4-byte field in the first `0x600` bytes of each object as a
possible `UObject*`, expanding only through classes whose name contains
`Inventory`, `Container`, `Weapon`, `Item`, `Power` or `Pawn`, to depth 3.

**It cannot traverse a `TArray`.** A `TArray` field's first four bytes are a
pointer to a heap buffer of elements, not a `UObject*`, so `LooksLikeObj`
rejects it and the walk stops there. Every inventory item lives in
`DishonoredInventory.m_Slots`, which is exactly such an array.

That is the measured consequence, over a full run: **all 19 published component
snapshots were identical**, six components, and the pistol never appeared at all
even while it was the weapon in hand (VR-60). A pointer-chasing walk that skips
arrays sees whatever happens to be reachable by direct pointer and nothing else,
and it cannot report what it missed, because it never knew it was there.

A walk like this is also unfalsifiable in the way this project has learned to
distrust: it produces a confident list, and a thing absent from that list is
indistinguishable from a thing that does not exist.

## 5. The technique: resolve properties by name

**This repo already has it, and it has since 38.x.** An earlier draft of this
document sent a session to the BioShock trilogy mod for the technique before
grepping here for prior art, which is backwards and is recorded so it is not
repeated.

### What exists, in `ue3/uobject.cpp`

| Helper | What it does |
|---|---|
| `FindNameIdx(name)` | text to FName index, by scanning `GNames` |
| `NameFromIndex` / `RealName` / `ObjClassName` | FName index to text, and any object to its class name |
| `FindPropOffset(className, propName)` | a property offset, by scanning `GObjects` |
| `FindBoolProp(className, propName, &off, &mask)` | the same for a bool, plus its bitmask |

**Why it needs no chain offsets at all.** Every `UProperty` is itself a
`UObject` whose `Outer` is the class that declares it. So a scan of `GObjects`
for (this name, that outer name, a class whose name contains `Property`) finds
the property object, and its own recorded offset is the answer. There is no
`Children` / `Next` / `SuperStruct` to derive, no candidate layout to get wrong
and no search to validate.

That is strictly more robust than walking the property chain, which is what the
trilogy mod had to do. `kUPropOffset` and `kUBoolBitMask` are in `patterns.h`;
ENGINE_NOTES records how the 38.x skelcontrol property dump derived them.

It is already load-bearing: `arm_follow.cpp` resolves twelve properties through
it, and `crouch.cpp`, `block_state.cpp` and `skelcontrol.cpp` use it too.

### The established idiom, which new code should follow

Resolve ONCE into a global, behind a GNames sanity gate - `NameFromIndex(0)`
must read `None`, because our DLL loads from `DllMain` during the exe's import
resolution, before the exe's CRT static initializers, so the name pool is empty
then. **Nothing may resolve at init.** `arm_follow.cpp` is the reference.

### What was actually missing, and is now in `ue3/reflect.cpp`

Two things, both small:

1. **A cache.** `FindPropOffset` is a full `GObjects` scan of hundreds of
   thousands of entries. Every existing caller memoises into a global by hand,
   which works but has to be got right each time. The trilogy mod measured what
   getting it wrong costs: a name scan on a poll cadence stuttered that whole
   game at 2-3 Hz, and its rule is **never on a cadence**. A cache makes the
   safe thing the default instead of a convention, and it caches misses too,
   since a miss costs a full scan as well.
2. **A TArray reader.** The capability VR-60 needs, and the one thing the
   component walk provably cannot do.

No new engine offsets were derived for either, and none were guessed.

### What is still worth taking from the trilogy mod

Not the resolver - the things layered on top of one, none of which exist here:
a UFunction call-by-name lane, a resolved-index form for cadenced callers,
`DynamicLoadObject` by path, and a UClass-fixpoint gate for deciding whether an
arbitrary pointer is a genuine UObject. Those are separate tickets if and when
something needs them.
## 6. Rules for this subsystem

Carried from the engineering rules this project already runs on.

1. **Never hardcode a property offset.** Derive it by name, log it once, refuse on
   drift. An address that must be constant goes in `patterns.h` with its
   derivation, byte-verified.
2. **Never copy a number from another game.** BioShock's slots are a scan window,
   not an answer.
3. **A flag is not a flag until a run can show it CHANGING.** A boolean that reads
   the same in every state is not evidence it is the right boolean; identify a
   flag by making it move, the same way a render pass is identified.
4. **Log state CHANGES, not state.** `equip: Secondary crossbow_01 -> pistol` earns
   a line; the same value every frame earns nothing.
5. **Read on the script lane, publish a snapshot, consume on the present lane.**
   The component snapshot already works this way and the lane contract in
   ARCHITECTURE applies unchanged.
6. **Every flag records its own freshness.** A stale flag consumed as current is
   the exact failure that cost VR-59 two builds: a reference has to be maintained
   on the path that uses it.
7. **Fail soft and fail INERT.** When state cannot be read, the mod does what it
   did before the flag existed. A flag that defaults to a confident wrong answer
   is worse than no flag.

## 7. What this unblocks

* **VR-60**, the pistol. Reading `m_Slots` gives every item and its hand directly,
  which is what the pointer walk cannot reach.
* **The arms during takedowns and chokes.** `eDisPlayerActionUsage_Fullbody` is
  the signal to unhide the arms and hand animation control back to the game.
* **The hand sides**, currently an assumption in `WaHandFor`, become measured
  attachment data from `m_RequiredUsage`.
* **Throwables in general.** The weapon attachment's instance rule (see the
  ARCHITECTURE decision log) is geometric today. Given equipment state it can ask
  the engine which object is in the hand instead of inferring it from position.
* **VR-51**, the pause-menu session loss, and any other bug where the mod needs to
  know what mode the game is in.

## 8. Related

* `docs/dishonored/ENGINE_NOTES.md` for what is derived on this build, including
  the loaded-versus-fired bolt class table and the inventory-walk limitation.
* `docs/ARCHITECTURE.md` decision log, "a contract identifies a geometry, never an
  instance", which is the same lesson one layer down.
* `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` section 8, the graveyard, for what
  filters built on inference cost in headset runs.
