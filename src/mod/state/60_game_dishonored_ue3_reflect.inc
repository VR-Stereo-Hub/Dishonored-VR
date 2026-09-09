// ---- VR-61: GAMEPLAY STATE, READ BY NAME ------------------------------------
//
// The argument is in docs/dishonored/GAMEPLAY_STATE.md: almost every hard bug in
// this mod came from acting on a guess about game state because there was no way
// to ask. This is the way to ask.
//
// THIS REPO ALREADY HAD THE RESOLVER, and the first version of this module
// reinvented it. `FindPropOffset(className, propName)` and `FindBoolProp` in
// ue3/uobject.cpp have resolved properties by name since 38.x and are used by
// arm_follow, crouch, block_state and skelcontrol - twelve properties in
// arm_follow alone. They need NO chain offsets: every UProperty is itself a
// UObject whose Outer is the class that declares it, so a scan of GObjects for
// (name, outer name, class contains "Property") finds it and its own recorded
// offset is the answer.
//
// That is a better foundation than walking Children/Next/SuperStruct, because it
// derives nothing: there is no candidate layout to get wrong and no search to
// validate. So this module adds only what was genuinely missing, and the two
// things it adds are the two the existing helper lacks.
//
// WHAT IS NEW HERE, AND WHY EACH ONE IS NEEDED:
//
// 1. A CACHE. FindPropOffset is a full GObjects scan of hundreds of thousands of
//    entries. Every existing caller resolves once into a global by hand, which
//    works but has to be re-done correctly each time. The BioShock trilogy mod
//    measured the cost of getting it wrong: a name scan on a poll cadence
//    stuttered that whole game at 2-3 Hz, and its rule is "never on a cadence".
//    A cache makes the safe thing the default instead of a convention.
//
// 2. A TArray READER. This is the capability VR-60 actually needs. Every
//    inventory item in this game lives in DishonoredInventory.m_Slots, a
//    TArray<PawnInventorySlot>, and FpCollect cannot traverse it: the walk reads
//    each 4-byte field as a UObject pointer, and a TArray field's first four
//    bytes are a heap data pointer. That is why the pistol never appears in a
//    component snapshot however long the run.
//
// No new engine offsets are derived and none are guessed. The two UProperty
// offsets this depends on are the ones the existing code already established,
// now in patterns.h where this repo requires engine offsets to live.

// A property offset, memoised. Keyed on the DECLARING class name, which is what
// FindPropOffset matches on - for a struct member that is the ScriptStruct
// itself, a trap arm_follow.cpp already documents.
struct RflProp {
    char     cls[56];
    char     prop[56];
    uint32_t off;      // 0 = looked up and genuinely not found
    bool     done;
};
#define RFL_PROP_MAX 48
static RflProp g_rflProp[RFL_PROP_MAX];
static int     g_rflPropN = 0;
static LONG    g_rflHit   = 0;   // answered from cache
static LONG    g_rflScan  = 0;   // full GObjects scans actually performed
static LONG    g_rflMiss  = 0;   // scans that found nothing

// ---- the state snapshot -----------------------------------------------------
//
// Read on the script lane and published for anyone who wants it. It carries its
// own freshness, because a stale flag consumed as current is the exact failure
// that cost VR-59 two builds (GAMEPLAY_STATE rule 6).
//
// EDisEquipUsage: 0 None, 1 Primary, 2 Secondary.
struct RflState {
    char     equip[3][64];   // item class and name, by equip usage
    int      slots;          // elements in m_Slots
    int      usable;         // slots that yielded a readable item
    double   readMs;         // when this was last read
    uint32_t gen;
    bool     ok;
};
static RflState g_rflState;
static char     g_rflEquipPrev[3][64] = {};
static bool     g_rflStateOn = true;      // [Hands] StateFlags
static double   g_rflStateMs = 0.0;
static char     g_rflWhy[224] = "not attempted yet";
static LONG     g_rflReads = 0;

// PawnInventorySlot is { DishonoredInventoryItem* m_pItem; Class* m_pRequiredType;
// EDisEquipUsage m_RequiredUsage; } - two pointers and a byte, padded. The STRIDE
// is the one number in this path that reflection cannot answer, because element
// layout is not a property. So it is VALIDATED rather than trusted: a candidate
// is accepted only if it yields readable UObject item pointers, and if none does
// the log says the stride is wrong rather than that the inventory is empty. Those
// two look identical without the distinction.
static const int kRflStrideCandidates[] = { 12, 16 };
static int g_rflStride       = 0;   // 0 until one validates
static const int kRflUsageOff = 8;  // the byte after the two pointers


// ---- THE ITEM ANSWERS FOR ITSELF --------------------------------------------
//
// The first run read m_Slots and found all four weapons - sword, powers,
// crossbow AND pistol, the last of which never appears in a component snapshot.
// But it read the SLOT for equipment state and that was the wrong field:
// PawnInventorySlot.m_RequiredUsage is a CONSTRAINT on what may occupy a slot,
// not what is in the hand. The dump showed it plainly - slot 0 required usage
// Primary and slot 1 required Secondary, both holding a DishonoredItemEmpty
// placeholder, while the real weapons sat in slots 2-5 with no constraint at
// all (usage 0).
//
// DishonoredInventoryItem carries the answer on itself:
//
//   m_EquipUsage : EDisEquipUsage   None | Primary | Secondary
//   m_CurSocket  : EItemSocket      None | Equipped | Holstered | Give
//
// So the item says which hand it is in and whether it is actually in that hand
// or on the body. That is the equipped-versus-holstered distinction VR-59
// attempt 1 needed and could not obtain from component presence, and it is a
// per-object fact rather than an inference from an index.
enum RflSocket {
    RFL_SOCKET_NONE = 0,
    RFL_SOCKET_EQUIPPED = 1,
    RFL_SOCKET_HOLSTERED = 2,
    RFL_SOCKET_GIVE = 3
};
static const char* RflSocketName(int v)
{
    switch (v) {
        case RFL_SOCKET_NONE:      return "none";
        case RFL_SOCKET_EQUIPPED:  return "EQUIPPED";
        case RFL_SOCKET_HOLSTERED: return "holstered";
        case RFL_SOCKET_GIVE:      return "give";
        default:                   return "?";
    }
}

// The equipped item per hand, and the object behind it - the object is what
// VR-60 needs, because its m_pPlayerMesh is the component the pistol should be
// verified against instead of another weapon's.
static uint8_t* g_rflHeldObj[3] = {};
static int      g_rflHeldSocket[3] = {};

// ---- THE EQUIPMENT REVISION -------------------------------------------------
//
// A number that changes when what is in the hands changes, and at no other time.
// It exists because the candidate list has to be refreshed on a swap, and the
// three things already available are all wrong for that job:
//
//   * the CLASS NAME comparison the equipment log uses. Two instances of the
//     same weapon are the same string and a different object, and the component
//     that must be re-collected belongs to the object.
//   * g_rflState.gen, which increments on every successful READ, four times a
//     second, whether anything changed or not.
//   * the equip[] strings, same problem as the first.
//
// So the signature is built from VALIDATED OBJECT IDENTITY - the item pointer
// each hand holds and the socket it reports - and the revision moves only when
// that signature does.
//
// A FAILED READ IS UNKNOWN, NOT EMPTY. The signature is only recomputed where
// the state read succeeded; a tick that could not resolve the inventory leaves
// the revision exactly where it was, so a momentary read failure cannot be
// mistaken for the player putting everything away.
static uint32_t g_rflEquipSig   = 0;      // hash of the validated identities
static uint32_t g_rflEquipRev   = 0;      // bumped when the signature changes
static double   g_rflEquipRevMs = 0.0;    // when, for the settle window
static bool     g_rflEquipSigOk = false;  // a signature has ever been computed
