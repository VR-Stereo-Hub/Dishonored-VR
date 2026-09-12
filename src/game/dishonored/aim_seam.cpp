// game/dishonored/aim_seam.cpp - VR-57/VR-34: WHERE DOES THE SHOT'S DIRECTION
// COME FROM? A read-only probe. It writes nothing the game reads.
//
// What the tester measured (2026-09-11): with [MotionAim] Enabled=0 the bolt
// goes where the game's own crosshair is, which is head aim; with it 1 the bolt
// leaves up and behind. So the engine computes the fire direction itself, and
// the old post-spawn steering is not the way to take it over.
//
// What the scripts say (decompiled, read only, never copied into this tree):
// the player's ranged item contexts derive from an AIM-ASSIST attack context,
// and it caches the assist's answer for the tick in one struct, resolved live
// at +0x00d0 on this build:
//
//     m_CachedAimAssistPos : { int m_TickTag; bool m_bFound; Vector m_AimPos;
//                              Vector m_AimDir; Vector2D m_ProjectedAimPos;
//                              bool m_bWillTrack; }
//
// A cached direction AND a projected screen position together are what a
// crosshair that walks onto enemies would be drawn from. If the fire path and
// the HUD both read this, then writing it drives the shot, the crosshair and
// the assist from one ray - the ONE RAY rule, with the game's own assist kept
// rather than fought.
//
// FIRST RUN, 2026-09-11 (build 102-g446431d9), and what it corrected:
//   * the struct resolves and reads;
//   * a GObjects scan for the exact class name found ONE object whose tick tag
//     never moved and whose fields were all zero - the class default object,
//     not a live context. The live ones are SUBCLASSES: the crossbow and the
//     pistol each have their own, and a name-equality scan cannot see them.
// So this pass reaches the context the way the game does: the pawn's inventory,
// the EQUIPPED item, and that item's primary player context slots. The scan
// stays only as a fallback, cheap (one verdict per class pointer, no name
// lookup per object) and stopped as soon as the inventory answers.
//
// Still a hypothesis until the numbers say otherwise: that this cache is what
// the fire path and the HUD read. What would kill it here: a tick tag that
// never advances on the equipped weapon's context, a layout that does not
// parse, or a direction that ignores the view.
//
// Bounded: armed by [Aim] SeamProbe (ships 0), four samples a second, the
// fallback scan at most every ten seconds and never once the inventory route
// works, every line rate-limited.

#define DVR_CAT ::dvr::log::Cat::script

// The struct, as the script declares it. Offsets are ASSUMED here and PRINTED
// beside a hex dump, so a wrong assumption shows up as unreadable numbers
// rather than as a confident wrong answer.
struct AsCache {
    int32_t  tickTag;
    uint32_t found;
    float    aimPos[3];
    float    aimDir[3];
    float    projected[2];
    uint32_t willTrack;
};
static const uint32_t kAsCacheBytes = 44;   // 4+4+12+12+8+4

static bool AsRead(uint8_t* p, AsCache* out)
{
    if (!p || !RangeReadable(p, kAsCacheBytes)) return false;
    out->tickTag = *(int32_t*)(p + 0x00);
    out->found   = *(uint32_t*)(p + 0x04);
    memcpy(out->aimPos,    p + 0x08, 12);
    memcpy(out->aimDir,    p + 0x14, 12);
    memcpy(out->projected, p + 0x20, 8);
    out->willTrack = *(uint32_t*)(p + 0x28);
    return true;
}

// Sane-value gate. It exists to make a WRONG LAYOUT loud: a direction that is
// not unit length, or a position the size of a galaxy, means these bytes are
// not the fields they are labelled with. An all-zero cache is not implausible,
// it is simply idle, and it is reported as idle rather than as a reading.
static bool AsPlausible(const AsCache& c, const char** why)
{
    const float dl = sqrtf(c.aimDir[0]*c.aimDir[0] + c.aimDir[1]*c.aimDir[1] +
                           c.aimDir[2]*c.aimDir[2]);
    for (int i = 0; i < 3; i++)
        if (!MpFinite(c.aimPos[i]) || fabsf(c.aimPos[i]) > 1.0e7f)
            { *why = "m_AimPos is not a world position"; return false; }
    if (!MpFinite(dl) || (dl > 0.001f && (dl < 0.5f || dl > 2.0f)))
        { *why = "m_AimDir is not a unit direction"; return false; }
    if (c.found > 1 || c.willTrack > 1)
        { *why = "the bool fields are not 0/1 - the layout is off"; return false; }
    *why = (dl < 0.001f && c.tickTag == 0) ? "idle (all zero: never filled)"
                                           : "plausible";
    return true;
}

// Per-pointer tick memory, so "advanced" is a fact about the object rather
// than an artefact of when the probe last looked it up.
static int32_t* AsTagSlot(uint8_t* obj)
{
    for (int i = 0; i < g_asN; i++) if (g_asObj[i] == obj) return &g_asTag[i];
    if (g_asN < kAsMax) {
        g_asObj[g_asN] = obj;
        g_asTag[g_asN] = 0x7fffffff;
        return &g_asTag[g_asN++];
    }
    return NULL;
}

static void AsLogOne(const char* label, uint8_t* obj, uint32_t off,
                     const float* viewF, const float* handF, bool haveHand)
{
    AsCache c;
    if (!AsRead(obj + off, &c)) return;
    int32_t* slot = AsTagSlot(obj);
    const bool moved = slot ? (c.tickTag != *slot) : false;
    if (slot) *slot = c.tickTag;

    const char* why = "?";
    const bool ok = AsPlausible(c, &why);
    const float dl = sqrtf(c.aimDir[0]*c.aimDir[0] + c.aimDir[1]*c.aimDir[1] +
                           c.aimDir[2]*c.aimDir[2]);
    float dv = -2.0f, dh = -2.0f;
    if (dl > 0.5f) {
        const float n[3] = { c.aimDir[0]/dl, c.aimDir[1]/dl, c.aimDir[2]/dl };
        dv = V3Dot(n, viewF);
        if (haveHand) dh = V3Dot(n, handF);
    }
    const char* cn = ObjClassName(obj);
    const char* nm = RangeReadable(obj + kNameOff, 4)
                   ? RealName(*(uint32_t*)(obj + kNameOff)) : NULL;
    Log("aimseam: %s | %s '%s' | tick %d %s | found=%u willTrack=%u | "
        "aimPos (%.0f %.0f %.0f) aimDir (%+.3f %+.3f %+.3f) projected (%.3f %.3f) | "
        "dot(view)=%+.3f dot(hand)=%+.3f | %s. A name beginning Default__ is the "
        "class default object and never fires; dot(view)=+1 means the cache "
        "follows the HEAD, which is what a view-derived shot looks like, and "
        "dot(hand) is how far the controller ray sits from it (-2 = no direction "
        "cached, or no controller pose).",
        label, cn ? cn : "?", nm ? nm : "?", c.tickTag,
        moved ? "ADVANCED since the last sample" : "unchanged",
        c.found, c.willTrack, c.aimPos[0], c.aimPos[1], c.aimPos[2],
        c.aimDir[0], c.aimDir[1], c.aimDir[2], c.projected[0], c.projected[1],
        dv, dh, why);
    if (moved && dl > 0.5f) g_asLiveSeen = true;

    if (!ok) {
        char hex[3 * kAsCacheBytes + 8];
        int n = 0;
        for (uint32_t i = 0; i < kAsCacheBytes && n < (int)sizeof(hex) - 4; i++)
            n += _snprintf(hex + n, sizeof(hex) - n, "%02x ", (obj + off)[i]);
        hex[sizeof(hex) - 1] = 0;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
            "aimseam: the %u bytes at the cache are %s- the labelled layout is "
            "(int tag)(bool found)(vec pos)(vec dir)(vec2 projected)(bool track) "
            "and it did not parse. Do not write into this until it does.",
            (unsigned)kAsCacheBytes, hex);
    }
}

// The equipped weapon's contexts, reached the way the game reaches them:
// pawn -> inventory -> slots -> the EQUIPPED item -> its primary player
// context slots. Returns how many contexts it read.
static int AsWalkInventory(uint32_t cacheOff, const float* viewF,
                           const float* handF, bool haveHand)
{
    uint8_t* pawn = FpPawn();
    if (!pawn) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
            "aimseam: no pawn latched - nothing to walk yet");
        return 0;
    }
    static const char* const kPawnClasses[] = { "DishonoredPawn", "DishonoredPlayerPawn" };
    const uint32_t invOff   = RflOffsetOfAny(kPawnClasses, 2, "m_pInventory", NULL);
    const uint32_t slotsOff = RflOffsetOf("DishonoredInventory", "m_Slots");
    const uint32_t ctxOff   = RflOffsetOf("DishonoredInventoryItem",
                                          "m_ContextSlots_Primary_Player");
    const uint32_t sockOff  = RflOffsetOf("DishonoredInventoryItem", "m_CurSocket");
    if (!invOff || !slotsOff || !ctxOff || !sockOff) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 10000,
            "aimseam: the inventory route did not resolve (inventory +0x%04x, "
            "slots +0x%04x, context slots +0x%04x, socket +0x%04x) - the rfl: "
            "lines above name which property refused",
            invOff, slotsOff, ctxOff, sockOff);
        return 0;
    }
    if (!RangeReadable(pawn + invOff, 4)) return 0;
    uint8_t* inv = *(uint8_t**)(pawn + invOff);
    if (!LooksLikeObj(inv)) return 0;
    uint8_t* data = NULL; int32_t num = 0;
    if (!RflArrayAt(inv, slotsOff, &data, &num)) return 0;

    int read = 0;
    const int nCand = (int)(sizeof(kRflStrideCandidates) / sizeof(kRflStrideCandidates[0]));
    for (int attempt = 0; attempt < (g_rflStride ? 1 : nCand) && !read; ++attempt) {
        const int s = g_rflStride ? g_rflStride : kRflStrideCandidates[attempt];
        for (int i = 0; i < num && i < 64; ++i) {
            uint8_t* slot = data + (size_t)i * (size_t)s;
            if (!RangeReadable(slot, (size_t)s)) break;
            uint8_t* item = *(uint8_t**)slot;
            if (!item || !LooksLikeObj(item)) continue;
            if (!RangeReadable(item + sockOff, 1)) continue;
            if ((int)*(uint8_t*)(item + sockOff) != RFL_SOCKET_EQUIPPED) continue;
            uint8_t* cdata = NULL; int32_t cnum = 0;
            if (!RflArrayAt(item, ctxOff, &cdata, &cnum)) continue;
            const char* icn = ObjClassName(item);
            for (int k = 0; k < cnum && k < 8; ++k) {
                if (!RangeReadable(cdata + (size_t)k * 4, 4)) break;
                uint8_t* ctx = *(uint8_t**)(cdata + (size_t)k * 4);
                if (!LooksLikeObj(ctx)) continue;
                char label[96];
                _snprintf(label, sizeof(label), "EQUIPPED %s primary context %d of %d",
                          icn ? icn : "?", k, cnum);
                label[sizeof(label) - 1] = 0;
                AsLogOne(label, ctx, cacheOff, viewF, handF, haveHand);
                ++read;
            }
        }
    }
    if (!read)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
            "aimseam: the inventory walked but no EQUIPPED item offered a primary "
            "player context (%d slot(s) seen). Holstered items are skipped on "
            "purpose; the fallback scan is what covers a wrong socket read.",
            (int)num);
    return read;
}

// The fallback: every live object whose class looks like a weapon item context.
// One verdict per CLASS POINTER, so the million-pointer walk costs no name
// lookups after the first object of each class - the first pass of this probe
// did one lookup per object, and that is a stall the tester can feel.
static void AsScan(uint32_t cacheOff, const float* viewF, const float* handF, bool haveHand)
{
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;
    if (!RangeReadable(objs, (size_t)num * sizeof(void*))) return;

    static void*   clsKey[2048] = {};
    static uint8_t clsVal[2048] = {};
    int hits = 0;
    for (uint32_t i = 1; i < num && hits < kAsMax; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3)) continue;
        if (!RangeReadable(o, kClassOff + 4)) continue;
        void* cls = *(void**)(o + kClassOff);
        if (!cls || ((uintptr_t)cls & 3)) continue;
        const unsigned h = (unsigned)(((uintptr_t)cls) >> 4) & 2047;
        bool match;
        if (clsKey[h] == cls) match = clsVal[h] != 0;
        else {
            const char* cn = ObjClassName(o);
            match = cn && strstr(cn, "ItemContext_") &&
                    (strstr(cn, "Fire") || strstr(cn, "ProjectileAttack") ||
                     strstr(cn, "Throw"));
            clsKey[h] = cls; clsVal[h] = match ? 1 : 0;
        }
        if (!match) continue;
        const char* nm = RangeReadable(o + kNameOff, 4)
                       ? RealName(*(uint32_t*)(o + kNameOff)) : NULL;
        if (nm && !strncmp(nm, "Default__", 9)) continue;   // the CDO never fires
        AsLogOne("scanned", o, cacheOff, viewF, handF, haveHand);
        ++hits;
    }
    Log("aimseam: fallback scan saw %d live weapon item context(s) (cap %d). It "
        "runs only while the inventory route has not produced a context whose "
        "tick tag advances.", hits, (int)kAsMax);
}

static void AimSeamTick(void)
{
    if (!g_asOn) return;
    const double now = MaimNowMs();
    if (now - g_asLastMs < 250.0) return;
    g_asLastMs = now;

    if (!RflNamesReady()) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
            "aimseam: GNames is not up yet - nothing resolves by name this early");
        return;
    }
    const uint32_t cacheOff = RflOffsetOf("DisItemContext_ProjectileAttack",
                                          "m_CachedAimAssistPos");
    if (!cacheOff) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 10000,
            "aimseam: m_CachedAimAssistPos did not resolve on "
            "DisItemContext_ProjectileAttack - the rfl: line says what the scan "
            "reported. Subclasses inherit it, so this one offset serves them all.");
        return;
    }

    // The two reference directions every sample is judged against.
    const float viewF[3] = { cosf(g_viewPitchRad) * cosf(g_viewYawRad),
                             cosf(g_viewPitchRad) * sinf(g_viewYawRad),
                             sinf(g_viewPitchRad) };
    float handF[3] = { 0, 0, 0 };
    float rel[3];
    const bool haveHand = MaimHandRel(rel);
    if (haveHand) MaimDirFromView(g_viewYawRad, g_viewPitchRad, rel, handF);

    const int fromInventory = AsWalkInventory(cacheOff, viewF, handF, haveHand);
    if (!g_asLiveSeen && (!fromInventory || g_asVerbose) &&
        now - g_asScanMs > 10000.0) {
        g_asScanMs = now;
        AsScan(cacheOff, viewF, handF, haveHand);
    }
}
