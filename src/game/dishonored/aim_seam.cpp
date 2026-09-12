// game/dishonored/aim_seam.cpp - VR-57/VR-34: WHERE DOES THE SHOT'S DIRECTION
// COME FROM? A read-only probe. It writes nothing the game reads.
//
// What the tester measured (2026-09-11): with [MotionAim] Enabled=0 the bolt
// goes where the game's own crosshair is, which is head aim; with it 1 the bolt
// leaves up and behind. So the engine computes the fire direction itself, and
// the old post-spawn steering is not the way to take it over.
//
// What the scripts say (decompiled, read only, never copied into this tree):
// the player's ranged item context derives from an AIM-ASSIST attack context,
// and it caches the assist's answer for the tick in one struct:
//
//     m_CachedAimAssistPos : { int m_TickTag; bool m_bFound; Vector m_AimPos;
//                              Vector m_AimDir; Vector2D m_ProjectedAimPos;
//                              bool m_bWillTrack; }
//
// A cached direction AND a projected screen position together are what a
// crosshair that walks onto enemies would be drawn from. If the fire path and
// the HUD both read this, then writing it drives the shot, the crosshair and
// the assist from one ray - which is exactly the ONE RAY rule, with the game's
// own assist preserved rather than fought.
//
// THAT IS A HYPOTHESIS. This probe is what makes it fail or hold before any
// write is written:
//   * does the struct resolve, and does its layout parse into sane numbers?
//   * does m_TickTag advance while the player aims (is this instance live)?
//   * does m_bFound turn true when the crosshair is over a person?
//   * does m_AimDir track the VIEW (the head) - the prediction if the shot
//     really is view-derived - and how far is it from the controller ray?
// A layout that parses to nonsense, or a tag that never moves, kills the plan
// here instead of in a headset.
//
// Bounded by construction: armed by [Aim] SeamProbe (ships 0), four samples a
// second, a GObjects rescan at most every five seconds, at most kAsMax
// instances tracked, and every line rate-limited.

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
// not the fields they are labelled with.
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
    *why = "plausible";
    return true;
}

static void AsLogOne(const char* label, uint8_t* obj, uint8_t* at,
                     const AsCache& c, const float* viewF, const float* handF,
                     bool haveHand, int32_t prevTag, bool moved)
{
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
    Log("aimseam: %s %s at +0x%04x | tick %d (%s) found=%u willTrack=%u | "
        "aimPos (%.0f %.0f %.0f) aimDir (%+.3f %+.3f %+.3f) |dir|=%.3f "
        "projected (%.3f %.3f) | dot(view)=%+.3f dot(hand)=%+.3f | %s. "
        "dot(view)=+1 means the cache follows the HEAD, which is what a "
        "view-derived shot looks like; dot(hand) is how far the controller ray "
        "is from it. A tick that never moves means this instance is not the "
        "one the player fires with.",
        label, cn ? cn : "?", (unsigned)(at - obj), c.tickTag,
        moved ? "advancing" : "UNCHANGED since the last sample", c.found,
        c.willTrack, c.aimPos[0], c.aimPos[1], c.aimPos[2],
        c.aimDir[0], c.aimDir[1], c.aimDir[2], dl,
        c.projected[0], c.projected[1], dv, dh, ok ? why : why);
    (void)prevTag;
    if (!ok) {
        char hex[3 * kAsCacheBytes + 8];
        int n = 0;
        for (uint32_t i = 0; i < kAsCacheBytes && n < (int)sizeof(hex) - 4; i++)
            n += _snprintf(hex + n, sizeof(hex) - n, "%02x ", at[i]);
        hex[sizeof(hex) - 1] = 0;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
            "aimseam: the %u bytes at the cache are %s- the labelled layout is "
            "(int tag)(bool found)(vec pos)(vec dir)(vec2 projected)(bool track) "
            "and it did not parse. Do not write into this until it does.",
            (unsigned)kAsCacheBytes, hex);
    }
}

// The player's projectile context, found two ways on purpose. The pawn's own
// attack context is the cheap handle but is only set while attacking; the
// GObjects scan finds the instance whether or not an attack is running, and
// disagreement between the two is itself a result.
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
            "DisItemContext_ProjectileAttack. Either the declaring class is a "
            "parent (the assist context) or the name differs on this build - "
            "check the rfl: line above for what the scan reported.");
        return;
    }

    // The two reference directions every sample is judged against.
    float viewF[3] = { cosf(g_viewPitchRad) * cosf(g_viewYawRad),
                       cosf(g_viewPitchRad) * sinf(g_viewYawRad),
                       sinf(g_viewPitchRad) };
    float handF[3] = { 0, 0, 0 };
    float rel[3];
    const bool haveHand = MaimHandRel(rel);
    if (haveHand) MaimDirFromView(g_viewYawRad, g_viewPitchRad, rel, handF);

    // (a) the pawn's current attack context
    uint8_t* pawn = FpPawn();
    if (pawn) {
        static const char* const kPawnClasses[] = { "DishonoredPawn",
                                                    "DishonoredPlayerPawn" };
        const char* which = NULL;
        const uint32_t ctxOff = RflOffsetOfAny(kPawnClasses, 2, "m_pAttackContext", &which);
        if (ctxOff && RangeReadable(pawn + ctxOff, 4)) {
            uint8_t* ctx = *(uint8_t**)(pawn + ctxOff);
            if (LooksLikeObj(ctx)) {
                AsCache c;
                if (AsRead(ctx + cacheOff, &c)) {
                    const bool moved = (c.tickTag != g_asPawnTag);
                    g_asPawnTag = c.tickTag;
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                        "aimseam: the pawn's attack context is live (declared on %s)",
                        which ? which : "?");
                    AsLogOne("pawn attack context", ctx, ctx + cacheOff, c,
                             viewF, handF, haveHand, g_asPawnTag, moved);
                }
            } else {
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                    "aimseam: the pawn holds no attack context right now "
                    "(+0x%04x reads null) - expected unless an attack is running; "
                    "the scan below does not depend on it", ctxOff);
            }
        }
    }

    // (b) every instance of the class, rescanned rarely and cached
    if (now - g_asScanMs > 5000.0 || !g_asN) {
        g_asScanMs = now;
        g_asN = 0;
        if (RangeReadable((void*)kGObjHdr, 12)) {
            void**   objs = *(void***)kGObjHdr;
            uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
            if (objs && !((uintptr_t)objs & 3) && num >= 2000 && num <= 4000000 &&
                RangeReadable(objs, (size_t)num * sizeof(void*))) {
                for (uint32_t i = 1; i < num && g_asN < kAsMax; i++) {
                    uint8_t* o = (uint8_t*)objs[i];
                    if (!o || ((uintptr_t)o & 3)) continue;
                    if (!RangeReadable(o, kClassOff + 4)) continue;
                    const char* cn = ObjClassName(o);
                    if (!cn || !strstr(cn, "ItemContext_ProjectileAttack")) continue;
                    g_asObj[g_asN] = o;
                    g_asTag[g_asN] = 0x7fffffff;
                    g_asN++;
                }
            }
        }
        Log("aimseam: %d instance(s) of the projectile attack context in "
            "GObjects (cap %d). The player's is the one whose tick tag advances "
            "while you aim; NPC contexts exist too and are the control.",
            g_asN, (int)kAsMax);
    }

    for (int i = 0; i < g_asN; i++) {
        uint8_t* o = g_asObj[i];
        if (!LooksLikeObj(o)) continue;
        AsCache c;
        if (!AsRead(o + cacheOff, &c)) continue;
        const bool moved = (c.tickTag != g_asTag[i]);
        g_asTag[i] = c.tickTag;
        if (!moved && !g_asVerbose) continue;   // quiet instances say nothing
        char label[32];
        _snprintf(label, sizeof(label), "instance %d", i);
        label[sizeof(label) - 1] = 0;
        AsLogOne(label, o, o + cacheOff, c, viewF, handF, haveHand, g_asTag[i], moved);
    }
}
