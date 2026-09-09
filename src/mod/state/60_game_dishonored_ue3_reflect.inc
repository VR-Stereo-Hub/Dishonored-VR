// ---- VR-61: THE UE3 PROPERTY RESOLVER ---------------------------------------
//
// Resolve a property BY NAME to its byte offset on this build, by walking the
// UClass property chain. The point is stated in GAMEPLAY_STATE.md: almost every
// hard bug in this mod so far came from acting on a guess about game state
// because there was no way to ask. This is the way to ask.
//
// WHAT IS ALREADY KNOWN, AND WHY THAT MAKES THIS CHEAP. UObject's own slots are
// derived on this build and live in patterns.h: kOuterOff 0x24, kNameOff 0x28,
// kClassOff 0x30. BioShock Infinite's equivalent work had to derive those first
// and called it the hard part. Here the anchor is already in hand, so only four
// slots are unknown: UField::Next, UStruct::SuperStruct, UStruct::Children and
// UProperty::Offset.
//
// THE ORACLE. Two component offsets on this build are measured and in use every
// frame - kWaComponentLocalToWorld 0x60 and kWaComponentTranslation 0x90 - and
// UE3 names those properties `LocalToWorld` and `Translation` on
// UPrimitiveComponent. So a candidate layout is accepted ONLY if the chain it
// produces resolves those two names to those two offsets. That is a
// self-validating search: it cannot quietly settle on a wrong layout, because a
// wrong layout does not reproduce an answer we already know.
//
// The alternative was brute force plus judgement, and this project has a rule
// about instruments that cannot fail their own hypothesis.
//
// TRAPS, all of them recorded by the BioShock trilogy mod's reflect.cpp and its
// ENGINE_NOTES, and all of them cost that project sessions:
//
//   * NEVER INIT-DRIVEN. Our DLL loads from DllMain during the exe's import
//     resolution, BEFORE the exe's CRT static initializers, so GNames is empty
//     at init time. Deriving at startup would fail every boot and look like a
//     broken instrument. This derives lazily, on first use, on the script lane.
//   * ObjectArchetype is class-classed and SHARED between objects, which is what
//     falsified it as a chain link there: two nodes cannot share a Next.
//   * A hash-next slot walks to unrelated hash neighbours and looks plausible.
//   * A leaf class may declare only a handful of properties, so the walk MUST
//     follow the super chain or it will report a class as having almost nothing.
//   * A full name-pool scan is hundreds of milliseconds. Doing one per poll
//     stuttered that game at 2-3 Hz. Here every resolve is cached and the walk
//     is one-shot per (class, name) pair - never on a cadence.
//
// Numbers are NOT carried across from that project. Its slot values informed the
// SHAPE of the search window and nothing else; this repo's rule against copying
// a number between games applies in full, and the oracle is what decides.

// The derived layout. -1 means not derived; the whole module refuses while any
// required slot is -1, and says which.
struct RflLayout {
    int next;        // UField::Next
    int super;       // UStruct::SuperStruct
    int children;    // UStruct::Children
    int propOffset;  // UProperty::Offset
    bool ok;
};
static RflLayout g_rfl = { -1, -1, -1, -1, false };

// Derivation bookkeeping, so a refusal can say what it tried.
static bool  g_rflTried      = false;  // a derivation attempt has completed
static int   g_rflCandidates = 0;      // layouts walked
static int   g_rflRejected   = 0;      // layouts that walked but failed the oracle
static char  g_rflWhy[192]   = "not derived yet (derives lazily on first use, "
                               "never at init - GNames is empty in DllMain)";
static void* g_rflOracleObj  = NULL;   // the component the oracle was proved on
static char  g_rflOracleCls[64] = "";

// The oracle's two answers, read from patterns.h rather than repeated here.
static const uint32_t kRflOracleL2W   = kWaComponentLocalToWorld;  // 0x60
static const uint32_t kRflOracleTrans = kWaComponentTranslation;   // 0x90

// Search windows. Deliberately generous and cheap: every walk bails on the
// first unreadable pointer, and the oracle rejects everything that is not the
// answer, so a wide window costs time once and buys not having to guess.
static const int kRflMinOff = 0x28;   // at or after UObject::Name
static const int kRflMaxOff = 0x9C;
static const int kRflStep   = 4;

// A resolve cache. Indices and offsets are stable for the process lifetime, and
// the walk is far too expensive to repeat.
struct RflCacheEntry {
    void*    cls;
    char     name[48];
    uint32_t off;
    uint32_t mask;      // bool properties only, 0 otherwise
    bool     found;
};
#define RFL_CACHE_MAX 64
static RflCacheEntry g_rflCache[RFL_CACHE_MAX];
static int           g_rflCacheN = 0;
static LONG          g_rflCacheHit  = 0;
static LONG          g_rflCacheMiss = 0;
static LONG          g_rflWalks     = 0;   // full chain walks performed
static LONG          g_rflRefused   = 0;   // resolves that found nothing

// How deep a super chain may go before we call it a cycle. UE3 hierarchies are
// shallow; a longer one means a bad link, not a deep class.
static const int kRflMaxSuper = 24;
// How many fields one class may declare before we call the chain broken. A wrong
// Next slot produces an endless walk through unrelated memory.
static const int kRflMaxFields = 4096;
// During the SEARCH a walk is capped far shorter. A wrong Next produces an
// endless plausible chain through unrelated memory, and the search pays that
// cost thousands of times rather than once.
static const int kRflSearchFields = 256;


// ---- THE FIRST CONSUMER: equipment, per hand --------------------------------
//
// This exists to make the resolver FALSIFIABLE. A resolver that derives a layout
// and reads nothing has proved only that it did not crash. Reading the inventory
// is the one thing the component walk provably cannot do (it cannot traverse a
// TArray, VR-60), so if this produces the equipped item per hand then the
// resolver has done something the previous instrument could not, and if it
// refuses it says which step failed with the numbers.
//
// Logs CHANGES only. The same equipment every tick earns nothing.
static bool  g_rflStateOn   = true;    // [Hands] StateFlags
static double g_rflStateMs  = 0.0;
static int   g_rflSlotStride = 12;     // PawnInventorySlot: item, type, usage
static char  g_rflEquip[3][64] = {};   // by EDisEquipUsage: None/Primary/Secondary
static char  g_rflEquipPrev[3][64] = {};
static int   g_rflSlotsSeen = 0;
static bool  g_rflStateOk   = false;
static char  g_rflStateWhy[192] = "not attempted yet";
static LONG  g_rflStateReads = 0;
