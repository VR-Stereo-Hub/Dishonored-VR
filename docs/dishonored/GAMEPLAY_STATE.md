# Gameplay state: the flags the mod needs, and how to read them

**Status: a WANTED RESOURCE, not yet built.** This document says what the mod
needs to know about what the game is doing, what the engine already exposes,
and which technique reads it reliably. Nothing here is implemented; where a
number appears it is a starting hypothesis to be re-derived against the running
build, never a value to copy.

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

Credited to the BioShock trilogy VR mod, where both halves are proven. Cite that
work; do not copy its numbers.

### Half one, already present here

FName index to text. `NameFromIndex` and `RealName` in `ue3/uobject.cpp` already
walk `GNames` to an `FNameEntry` and return its string, and `ObjClassName` gives
any object's class name. BioShock Remastered VR uses the same technique
(`fname_text` in its `patterns.cpp`) to name per-weapon profile keys and bones.

### Half two, missing here

**Name to property OFFSET, by walking the `UClass` property chain.** BioShock
Infinite measured this live on its own UE3 build: `UObject::Class`,
`UField::Next`, `UStruct::SuperStruct`, `UStruct::Children`, `UProperty::Offset`
and `UBoolProperty::BitMask`, exposed as `find_property_offset` /
`find_bool_property_bit`, self-deriving every boot and refusing on drift.

Dishonored is UE3 build 9099, a different build, so **every one of those slots
must be re-derived here.** Their Infinite values are a starting hypothesis for a
scan window and nothing more; this project's rule against copying a number
between games applies with full force.

### Why this is the right shape

* It is **pure pointer reading**. A wrong guess yields a bad number, not a crash,
  which is the opposite of the `ProcessEvent` plus `GetPropertyText` route.
* It is **self-validating**. We already know offsets for this build
  (`kWaComponentLocalToWorld = 0x60`, `kWaComponentTranslation = 0x90`,
  `kNameOff` / `kClassOff` / `kOuterOff` in `patterns.h`). A candidate layout is
  accepted only if the chain it produces reproduces those known answers. It
  cannot silently settle on a wrong layout.
* It **derives at boot rather than hardcoding**, so a patch that moves a field is
  a refusal with a logged reason instead of silent garbage.
* It replaces guessing with asking, which is the whole point of section 1.

### The shape of the API this should expose

```
resolve(obj, "PropertyName")     -> offset, or a refusal with a reason
read_int / read_float / read_bool / read_object / read_name / read_array
state()                          -> a snapshot of the flags in section 2
```

Every accessor guarded, every refusal logged with the values that caused it, and
a boot-time self-test that prints the derived layout next to the known offsets it
was validated against. **An instrument that cannot fail its own hypothesis is not
evidence**, so the self-test must be able to print the unwelcome answer.

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
