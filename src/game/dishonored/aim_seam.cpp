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


// Only the PROJECTILE family owns this cache. The sword's contexts live at the
// same offset with different fields, and reading them produced the nonsense
// ("m_AimPos is not a world position") in the 2026-09-11 run. Name-based, and
// the name is printed, so a class this misses is visible rather than silent.
static bool AsIsProjectileCtx(const char* cn)
{
    return cn && strstr(cn, "ItemContext_") &&
           (strstr(cn, "FireCrossbow") || strstr(cn, "FirePistol") ||
            strstr(cn, "ProjectileAttack") || strstr(cn, "Throw"));
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
    if (!g_asOn) return;   // drive-only: the walk still latched the context above
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
                const char* ccn = ObjClassName(ctx);
                if (!AsIsProjectileCtx(ccn)) {
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 10000,
                        "aimseam: EQUIPPED %s context %d is %s - not a projectile "
                        "context, so this offset holds a different field on it and "
                        "is not read", icn ? icn : "?", k, ccn ? ccn : "?");
                    continue;
                }
                char label[96];
                _snprintf(label, sizeof(label), "EQUIPPED %s primary context %d of %d",
                          icn ? icn : "?", k, cnum);
                label[sizeof(label) - 1] = 0;
                g_asCtx = ctx; g_asCtxOff = cacheOff; g_asCtxMs = MaimNowMs();
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

// WHERE EACH CONTROLLER ACTUALLY POINTS, in degrees off the head. Four rays -
// both hands, aim pose and grip pose - as azimuth (positive = the head's
// RIGHT) and elevation (positive = up). The tester reports the beam sitting
// about 45 degrees left and 10 to 20 degrees up from where the controller is
// aiming, and no amount of reading settles which ray is which: this prints all
// four next to the hand the beam and the write are using, so the one that
// reads near zero while the controller points straight ahead is named.
static void AsLogPoses(void)
{
    dvr::vr::HeadPose head;
    if (!dvr::vr::peek_head_pose(head)) return;
    const float fwdLocal[3] = { 0.0f, 0.0f, -1.0f };
    float headFwd[3];
    dvr::xrmath::quat_rotate(head.qx, head.qy, head.qz, head.qw, fwdLocal, headFwd);
    if (V3Norm(headFwd) < 0.5f) return;
    const float upW[3] = { 0.0f, 1.0f, 0.0f };
    float right[3]; V3Cross(headFwd, upW, right);
    if (V3Norm(right) < 0.2f) return;
    float up[3]; V3Cross(right, headFwd, up); V3Norm(up);

    char line[640]; int n = 0; line[0] = 0;
    // POSITIONS FIRST. Direction has been measured and looks right (a
    // controller held straight ahead reads az +1 el -3), so the remaining way
    // for the beam to miss is WHERE its dots are placed: they sit at the aim
    // pose's own position plus the ray, in XR LOCAL metres, and if that origin
    // is not the controller the far dot lands somewhere unrelated while the
    // direction is still correct.
    {
        float ap[3], aq[4];
        const int h = dvr::aim::config().hand;
        const bool have = dvr::vr::input_get_hand_pose(h, true, ap, aq);
        float d[3] = {0, 0, 0};
        if (have) dvr::xrmath::quat_rotate(aq[0], aq[1], aq[2], aq[3], fwdLocal, d);
        const float dist = dvr::aim::config().distanceM;
        n += _snprintf(line + n, sizeof(line) - n,
                       "head xr (%+.2f %+.2f %+.2f) | %s aim xr (%+.2f %+.2f %+.2f) "
                       "%s| far dot lands at (%+.2f %+.2f %+.2f), which is (%+.2f %+.2f %+.2f) "
                       "from the head | ", head.px, head.py, head.pz, h ? "R" : "L",
                       have ? ap[0] : 0.0f, have ? ap[1] : 0.0f, have ? ap[2] : 0.0f,
                       have ? "" : "(NO POSE) ",
                       ap[0] + dist * d[0], ap[1] + dist * d[1], ap[2] + dist * d[2],
                       ap[0] + dist * d[0] - head.px, ap[1] + dist * d[1] - head.py,
                       ap[2] + dist * d[2] - head.pz);
    }
    for (int hand = 0; hand < 2; ++hand) {
        for (int aimPose = 1; aimPose >= 0; --aimPose) {
            float pos[3], q[4];
            if (!dvr::vr::input_get_hand_pose(hand, aimPose != 0, pos, q)) {
                n += _snprintf(line + n, sizeof(line) - n, "%s%s %s: no pose",
                               n ? " | " : "", hand ? "R" : "L", aimPose ? "aim" : "grip");
                continue;
            }
            float d[3];
            dvr::xrmath::quat_rotate(q[0], q[1], q[2], q[3], fwdLocal, d);
            if (V3Norm(d) < 0.5f) continue;
            const float f = V3Dot(d, headFwd), r = V3Dot(d, right), u = V3Dot(d, up);
            const float az = atan2f(r, f) * 57.2957795f;
            const float el = asinf(u < -1.0f ? -1.0f : (u > 1.0f ? 1.0f : u)) * 57.2957795f;
            n += _snprintf(line + n, sizeof(line) - n,
                           "%s%s %s az %+.0f el %+.0f", n ? " | " : "",
                           hand ? "R" : "L", aimPose ? "aim" : "grip", az, el);
        }
    }
    line[sizeof(line) - 1] = 0;
    Log("aimseam/poses: %s || the beam and the write both use the %s hand's AIM "
        "pose. Point that controller straight ahead: its az and el should read "
        "near zero. A ray reading about -45 az is pointing at the head's LEFT, "
        "which is what the beam looks like from the report.",
        line, dvr::aim::config().hand ? "RIGHT" : "LEFT");
}

static void AimSeamTick(void)
{
    if (!g_asOn && !g_asDrive) return;   // the drive needs the walk to latch the context
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

    if (now - g_asPoseLogMs > 1000.0) { g_asPoseLogMs = now; AsLogPoses(); }

    const int fromInventory = AsWalkInventory(cacheOff, viewF, handF, haveHand);
    if (!g_asLiveSeen && (!fromInventory || g_asVerbose) &&
        now - g_asScanMs > 10000.0) {
        g_asScanMs = now;
        AsScan(cacheOff, viewF, handF, haveHand);
    }
}


// Refusals, counted per REASON. The previous build printed one "why" string
// beside a total, so 1,125,530 refusals were reported next to the word
// "writing" - a count whose population was unknowable, which is the exact
// failure this project keeps writing down.
static void AsRefuse(const char* why)
{
    ++g_asWriteRefused;
    g_asWriteWhy = why;
    for (int i = 0; i < g_asWhyN; i++)
        if (g_asWhyStr[i] == why) { ++g_asWhyCnt[i]; return; }
    if (g_asWhyN < kAsWhyMax) { g_asWhyStr[g_asWhyN] = why; g_asWhyCnt[g_asWhyN] = 1; ++g_asWhyN; }
}

static void AsWhySummary(char* out, size_t cap)
{
    int n = 0;
    out[0] = 0;
    for (int i = 0; i < g_asWhyN && n < (int)cap - 32; i++)
        n += _snprintf(out + n, cap - n, "%s%s x%ld", n ? ", " : "", g_asWhyStr[i], g_asWhyCnt[i]);
    out[cap - 1] = 0;
}

// THE HAND RAY IN THE GAME'S WORLD AXES, built from the runtime's AIM pose.
//
// The 2026-09-11 drive run wrote directions with a large +Z (up) component -
// dir (+0.491 -0.119 +0.863), dot(view) +0.51 - which is the "up and behind"
// the old motion aim has always shot. The cause is the ray it was built from:
// MaimHandRel -> HandRelFull reads g_devPose, which present_tick fills from
// the GRIP pose, then tilts it by [MotionAim] PitchOffsetDeg (40 degrees) and
// can mirror it. The grip is the handle, not a pointing axis.
//
// This builds the ray the runtime already publishes for pointing, with no
// offset and no flips, and expresses it in the same head-relative triple the
// game-space mapping wants: {right, up, forward} against a ROLL-FREE head
// frame, because the game camera has no roll.
//
// THE CHECK THAT CAN FAIL IT: the angle between the aim ray and the head's
// forward in XR space must equal the angle between the direction this returns
// and the view's forward in game space. Those are the same physical angle
// measured on both sides of the mapping, so a disagreement is proof the
// mapping is wrong, not an opinion about where a hand looks.
static float* g_asRelOut = NULL;   // the head-relative triple, for the log
static bool AsHandDirGame(float* dirOut, float* offsetGameOut, float* xrDeg,
                          float* gameDeg, const char** why)
{
    dvr::vr::HeadPose head;
    if (!dvr::vr::peek_head_pose(head)) { *why = "no head pose"; return false; }
    const int hand = dvr::aim::config().hand;
    float apos[3], aq[4];
    if (!dvr::vr::input_get_hand_pose(hand, true, apos, aq)) {
        *why = "no AIM pose for that hand"; return false;
    }
    const float fwdLocal[3] = { 0.0f, 0.0f, -1.0f };
    float aim[3], headFwd[3];
    dvr::xrmath::quat_rotate(aq[0], aq[1], aq[2], aq[3], fwdLocal, aim);
    dvr::xrmath::quat_rotate(head.qx, head.qy, head.qz, head.qw, fwdLocal, headFwd);
    if (V3Norm(aim) < 0.5f || V3Norm(headFwd) < 0.5f) {
        *why = "a pose rotated to a degenerate direction"; return false;
    }

    // The roll-free head frame, in XR axes (right +X, up +Y, forward -Z).
    const float upW[3] = { 0.0f, 1.0f, 0.0f };
    float right[3]; V3Cross(headFwd, upW, right);
    if (V3Norm(right) < 0.2f) { *why = "the head is looking straight up or down"; return false; }
    float up[3]; V3Cross(right, headFwd, up); V3Norm(up);

    // {right, up, forward} against that frame. In XR's right-handed axes
    // (forward x worldUp) already points RIGHT, which is why HandRelFull takes
    // this dot unnegated too; [MotionAim] FlipRight exists because that sign
    // was once in doubt, and it stays 0 here.
    //
    // NOTE what the angle check below CANNOT catch: a left/right mirror has the
    // same angle off the head, so it passes. The headset is what decides that
    // one - a mirrored ray sends the shot to the wrong side of the view.
    float rel[3] = { V3Dot(aim, right), V3Dot(aim, up), V3Dot(aim, headFwd) };
    MaimDirFromView(g_viewYawRad, g_viewPitchRad, rel, dirOut);
    const float n = V3Norm(dirOut);
    if (!(n > 0.5f && n < 2.0f)) { *why = "the mapped direction is not unit"; return false; }

    const float viewF[3] = { cosf(g_viewPitchRad) * cosf(g_viewYawRad),
                             cosf(g_viewPitchRad) * sinf(g_viewYawRad),
                             sinf(g_viewPitchRad) };
    const float cx = V3Dot(aim, headFwd), cg = V3Dot(dirOut, viewF);
    if (xrDeg)   *xrDeg   = acosf(cx < -1.0f ? -1.0f : (cx > 1.0f ? 1.0f : cx)) * 57.2957795f;
    if (gameDeg) *gameDeg = acosf(cg < -1.0f ? -1.0f : (cg > 1.0f ? 1.0f : cg)) * 57.2957795f;
    if (g_asRelOut) { g_asRelOut[0] = rel[0]; g_asRelOut[1] = rel[1]; g_asRelOut[2] = rel[2]; }

    // THE ORIGIN, and why the beam and the bolt disagreed. The shot was aimed
    // at a point measured from the CAMERA while the beam is drawn from the
    // CONTROLLER. Two parallel rays from origins ~40 cm apart hit different
    // places, and the closer the target the worse it is - which is exactly the
    // "sometimes a bit off" in the 2026-09-11 run. So the hand's offset from
    // the head travels with the direction, in the same head-relative triple
    // mapped by the same basis, and the aim point is measured from THERE.
    if (offsetGameOut) {
        const float dxr[3] = { apos[0] - head.px, apos[1] - head.py, apos[2] - head.pz };
        const float relPos[3] = { V3Dot(dxr, right), V3Dot(dxr, up), V3Dot(dxr, headFwd) };
        const float cp = cosf(g_viewPitchRad), sp = sinf(g_viewPitchRad);
        const float cy = cosf(g_viewYawRad),   sy = sinf(g_viewYawRad);
        const float F[3] = {  cp*cy,  cp*sy,  sp };
        const float R[3] = { -sy,     cy,     0  };
        const float U[3] = { -sp*cy, -sp*sy,  cp };
        const float uuPerM = (g_posScaleUU > 1.0f) ? g_posScaleUU : 100.0f;
        for (int i = 0; i < 3; i++)
            offsetGameOut[i] = (F[i]*relPos[2] + R[i]*relPos[0] + U[i]*relPos[1]) * uuPerM;
    }
    *why = "ready";
    return true;
}

// ---- DRIVING IT (VR-57 step 3, [Aim] DriveFromHand) -------------------------
//
// The probe established (2026-09-11, build 103-gdf52d9f3) that the equipped
// crossbow's context caches the assist's answer every tick, that its direction
// sits on the VIEW (dot +0.996 to +0.998 across every sample that found a
// target), and that it carries a projected screen point. This writes the
// controller ray into that cache instead.
//
// What it writes: the found flag, the aim position (the camera's own render
// position plus the ray at [Aim] DriveDistanceUU) and the aim direction. What
// it deliberately does NOT write:
//   * m_TickTag - the game's freshness stamp. Inventing one could read as
//     this-tick or as stale to code that has not been read; leaving it alone
//     means the write rides whatever the game already considers current.
//   * m_ProjectedAimPos - the crosshair's own placement stays the game's.
//     Whether the crosshair follows anyway is one thing the run answers.
//   * m_bWillTrack - homing stays off.
//
// It runs on the script lane on EVERY dispatch rather than on the probe's
// quarter second, because a shot can leave on any tick and a cache written
// four times a second would be stale for most of them.
//
// It can be wrong in a way this cannot see: the fire path may recompute the
// assist rather than read this cache. That is what the run decides. The
// read-back below reports how often the game overwrote the previous write,
// which is expected every tick and is NOT evidence either way by itself.
static void AimSeamDrive(void)
{
    if (!g_asDrive) return;
    // GAMEPLAY ONLY. The 2026-09-11 run crashed seven seconds after the pause
    // menu opened, and the same instant the log shows the game leaving
    // gameplay: weapon contracts dropped, hand candidates dropped. Those are
    // raw pointers into objects the game destroys, and this module holds one
    // too. A destroyed object's memory usually stays mapped, so it still reads
    // as plausible - which is how a write into freed memory happens without a
    // single guard noticing. The pointer is dropped with the rest of them.
    if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) {
        if (g_asCtx) {
            g_asCtx = NULL; g_asCtxOff = 0;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                "aimseam/drive: the game left gameplay - the context pointer is "
                "dropped rather than written to (a destroyed object still reads "
                "as plausible while its memory stays mapped)");
        }
        AsRefuse("not gameplay");
        return;
    }
    uint8_t* ctx = g_asCtx;
    if (!ctx || !g_asCtxOff) { AsRefuse("no equipped projectile context yet"); return; }
    if (!LooksLikeObj(ctx)) { g_asCtx = NULL; AsRefuse("the context stopped reading as a UObject"); return; }
    if (!AsIsProjectileCtx(ObjClassName(ctx))) { g_asCtx = NULL; AsRefuse("the context changed class"); return; }

    float dir[3], relDbg[3] = {0, 0, 0}, handOff[3] = {0, 0, 0};
    float xrDeg = -1.0f, gameDeg = -1.0f;
    const char* rayWhy = "?";
    g_asRelOut = relDbg;
    if (!AsHandDirGame(dir, handOff, &xrDeg, &gameDeg, &rayWhy)) {
        AsRefuse(rayWhy); return;
    }
    // The mapping's own falsification. If the angle off the head in XR and the
    // angle off the view in the game disagree, the direction below is wrong
    // whatever it looks like, so it is not written.
    if (fabsf(xrDeg - gameDeg) > 5.0f) {
        AsRefuse("the XR and game angles disagree - the mapping is wrong");
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 2000,
            "aimseam/drive: REFUSED - the aim ray sits %.1f deg off the head in XR "
            "but the mapped direction sits %.1f deg off the view in the game. Those "
            "are the same physical angle, so the mapping is wrong and nothing is "
            "written.", xrDeg, gameDeg);
        return;
    }
    float cam[3];
    if (!dvr::camera::render_pos(cam)) { AsRefuse("no camera position published yet"); return; }

    const float viewFwd[3] = { cosf(g_viewPitchRad) * cosf(g_viewYawRad),
                               cosf(g_viewPitchRad) * sinf(g_viewYawRad),
                               sinf(g_viewPitchRad) };
    uint8_t* at = ctx + g_asCtxOff;
    if (!RangeReadable(at, kAsCacheBytes)) { AsRefuse("the cache is not readable"); return; }

    // Read before writing: did the game overwrite what we put there last time?
    AsCache before;
    const bool haveBefore = AsRead(at, &before);
    if (haveBefore && g_asWrites > 0) {
        float d = 0.0f;
        for (int i = 0; i < 3; i++) d += fabsf(before.aimDir[i] - g_asLastDir[i]);
        if (d > 0.01f) ++g_asOverwritten;
    }


    // THE CROSSHAIR'S OWN FIELD. Without it the HUD kept computing its own
    // point while the shot followed ours, which is what "the crosshair went
    // off doing its own thing" was. The formula is normalised device
    // coordinates: the ray's offset from the view axis over the half-FOV
    // tangent, vertical scaled by the render's aspect.
    //
    // It is CALIBRATED against the game's own numbers rather than asserted: on
    // every tick the game refills the cache itself, the same formula is run
    // against ITS direction and compared with ITS projected point, and the
    // error is logged. A formula that disagrees with the engine's own answer is
    // wrong however plausible it looks.
    float proj[2] = { 0.0f, 0.0f };
    bool projOk = false;
    {
        float fovDeg = dvr::camera::rendered_fov_deg();
        if (!(fovDeg > 20.0f && fovDeg < 170.0f)) fovDeg = dvr::camera::fov_deg();
        if (fovDeg > 20.0f && fovDeg < 170.0f) {
            const float tanH = tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
            const uint32_t rw = dvr::capture::width(), rh = dvr::capture::height();
            const float aspect = (rw > 0 && rh > 0) ? (float)rh / (float)rw : 1.0f;
            const float tanV = tanH * aspect;
            float right[3], up[3];
            const float upW[3] = { 0.0f, 0.0f, 1.0f };   // game axes: Z is up
            V3Cross(viewFwd, upW, right);
            if (V3Norm(right) > 0.2f) {
                V3Cross(right, viewFwd, up); V3Norm(up);
                // the engine's right row points the other way round the cross
                right[0] = -right[0]; right[1] = -right[1]; right[2] = -right[2];
                const float f = V3Dot(dir, viewFwd);   // the ray's own depth in view axes
                if (f > 0.15f && tanH > 0.01f && tanV > 0.01f) {
                    proj[0] = (V3Dot(dir, right) / f) / tanH;
                    proj[1] = (V3Dot(dir, up) / f) / tanV;
                    projOk = MpFinite(proj[0]) && MpFinite(proj[1]) &&
                             fabsf(proj[0]) < 8.0f && fabsf(proj[1]) < 8.0f;
                }
                // the calibration: the same formula on the GAME's own sample
                if (haveBefore && before.found && projOk) {
                    const float bl = sqrtf(V3Dot(before.aimDir, before.aimDir));
                    const float bf = (bl > 0.5f) ? V3Dot(before.aimDir, viewFwd) / bl : 0.0f;
                    if (bf > 0.15f) {
                        const float bx = (V3Dot(before.aimDir, right) / bl / bf) / tanH;
                        const float by = (V3Dot(before.aimDir, up) / bl / bf) / tanV;
                        g_asProjErr = fabsf(bx - before.projected[0]) +
                                      fabsf(by - before.projected[1]);
                        g_asProjMine[0] = bx; g_asProjMine[1] = by;
                        g_asProjGame[0] = before.projected[0];
                        g_asProjGame[1] = before.projected[1];
                        g_asProjSamples++;
                    }
                }
            }
        }
    }

    // The aim point the beam's own dot sits on: the CONTROLLER's position, along
    // the ray, at the distance the crosshair lever draws that dot. Zero in
    // DriveDistanceUU means "follow the beam", which is the only setting that
    // cannot disagree with what the player sees.
    const float uuPerM = (g_posScaleUU > 1.0f) ? g_posScaleUU : 100.0f;
    const float distUU = (g_asDriveDistUU > 0.0f)
                       ? g_asDriveDistUU
                       : dvr::aim::config().distanceM * uuPerM;
    const float origin[3] = { cam[0] + handOff[0], cam[1] + handOff[1], cam[2] + handOff[2] };
    const float pos[3] = { origin[0] + dir[0] * distUU,
                           origin[1] + dir[1] * distUU,
                           origin[2] + dir[2] * distUU };
    *(uint32_t*)(at + 0x04) = 1u;          // m_bFound
    memcpy(at + 0x08, pos, 12);            // m_AimPos
    memcpy(at + 0x14, dir, 12);            // m_AimDir
    if (projOk) memcpy(at + 0x20, proj, 8);   // m_ProjectedAimPos: the crosshair
    *(uint32_t*)(at + 0x28) = 0u;          // m_bWillTrack: no homing
    memcpy(g_asLastDir, dir, sizeof(g_asLastDir));
    ++g_asWrites;
    g_asWriteWhy = "writing";

    const double now = MaimNowMs();
    if (now - g_asWriteLogMs > 1000.0) {
        g_asWriteLogMs = now;
        const float viewF[3] = { cosf(g_viewPitchRad) * cosf(g_viewYawRad),
                                 cosf(g_viewPitchRad) * sinf(g_viewYawRad),
                                 sinf(g_viewPitchRad) };
        const char* cn = ObjClassName(ctx);
        char whySummary[256]; AsWhySummary(whySummary, sizeof(whySummary));
        Log("aimseam/drive: wrote the AIM-pose ray into %s tick %d | dir (%+.3f %+.3f %+.3f) "
            "dot(view)=%+.3f | rel right/up/fwd (%+.3f %+.3f %+.3f) | off the head in XR "
            "%.1f deg, off the view in game %.1f deg (these must agree; that is the "
            "mapping's own check, and it does NOT catch a left/right mirror) | "
            "aimPos (%.0f %.0f %.0f) at %.0f uu along the ray from the CONTROLLER "
            "(its offset from the head is %.0f uu) | "
            "projected (%.3f %.3f)%s | %ld write(s), %ld refused [%s], the game "
            "rewrote the cache under us %ld time(s). Projection check against the "
            "engine's own sample: mine (%.3f %.3f) vs its (%.3f %.3f), error %.4f "
            "over %ld sample(s) - above about 0.05 the crosshair formula is wrong. Rewrites are EXPECTED (it recomputes every tick) and are "
            "not evidence by themselves. What the run decides: whether the BOLT "
            "follows this ray or still the crosshair - if it still follows the "
            "crosshair, the fire path does not read this cache and the seam is "
            "elsewhere.",
            cn ? cn : "?", haveBefore ? before.tickTag : -1,
            dir[0], dir[1], dir[2], V3Dot(dir, viewF), relDbg[0], relDbg[1], relDbg[2],
            xrDeg, gameDeg,
            pos[0], pos[1], pos[2],
            (double)distUU,
            (double)sqrtf(handOff[0]*handOff[0] + handOff[1]*handOff[1] + handOff[2]*handOff[2]),
            (double)proj[0], (double)proj[1],
            projOk ? "" : " NOT WRITTEN (behind the view or no FOV)",
            g_asWrites, g_asWriteRefused, whySummary, g_asOverwritten,
            (double)g_asProjMine[0], (double)g_asProjMine[1],
            (double)g_asProjGame[0], (double)g_asProjGame[1],
            (double)g_asProjErr, g_asProjSamples);
    }
}
