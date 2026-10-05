// Shared attachment follow for Heart, Possession, Blink and other hand particle/light components.
// Draw corrections arrive in camera-relative space. Bring that correction back to arm-local
// space, then place it in the CURRENT native parent frame before converting to bone-relative space.
// A saved world-space origin belongs to an older render/snapshot; mixing it with current bones
// adds a locomotion-dependent error. Each parent uses its own live frame, so updating the arm
// and held item at different points in the tick cannot turn body travel into a relative offset.
// Engine bone natives remain the source of the socket; never infer it from our previous write.
#include "fx_follow_frame.h"

#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands

struct FxEntry {
    uint8_t* comp;
    dvr::menukeep::Identity compId, meshId;
    uint32_t bone[2];
    float t0[3]; int32_t r0[3];
    float tw[3]; int32_t rw[3];
    bool wrote;
    double seenMs;
    int hand;
    float lastMove;
};
static FxEntry g_fx[24];
static int g_fxN = 0;
static uint32_t g_fxStride = 0;
static uint32_t g_fxAttOff = 0, g_fxL2W = kWaComponentLocalToWorld, g_fxLightL2W = 0;
static volatile LONG g_fxDriven = 0, g_fxRestored = 0, g_fxRejected = 0;
static const char* volatile g_fxWhy = "not asked yet";
static dvr::menukeep::Identity g_fxFnFrom, g_fxFnTo;
static uint32_t g_fxPublicationCutoff = 0;
static bool g_fxRequireNewPublication = false;
static float g_fxParentTravel[2] = {};

static bool FxSameId(const dvr::menukeep::Identity& a, const dvr::menukeep::Identity& b)
{ return a.obj && a.obj == b.obj && a.cls == b.cls && a.name[0] == b.name[0] && a.name[1] == b.name[1]; }
static bool FxReadId(uint8_t* object, dvr::menukeep::Identity* out)
{
    *out = {};
    if (!IsLiveObject(object)) return false;
    MkReadIdentity(object, out);
    return out->obj && out->cls;
}
static bool FxLiveId(const dvr::menukeep::Identity& id)
{
    dvr::menukeep::Identity now;
    return FxReadId((uint8_t*)id.obj, &now) && FxSameId(id, now);
}
static FxEntry* FxFind(uint8_t* comp)
{
    for (int i = 0; i < g_fxN; ++i) if (g_fx[i].comp == comp) return &g_fx[i];
    return nullptr;
}

// Re-read the current array and the complete relationship before every write. ProcessEvent
// may re-enter engine code, so an element address captured before a bone call is not retained.
static uint8_t* FxAttachment(const FxEntry& f)
{
    if (!g_fxAttOff || !g_fxStride || !FxLiveId(f.compId) || !FxLiveId(f.meshId)) return nullptr;
    auto* mesh = (uint8_t*)f.meshId.obj;
    if (!RangeReadable(mesh + g_fxAttOff, 12)) return nullptr;
    auto* data = *(uint8_t**)(mesh + g_fxAttOff);
    const int n = *(int*)(mesh + g_fxAttOff + 4);
    if (!data || n <= 0 || n > 64 || !RangeReadable(data, n * g_fxStride)) return nullptr;
    for (int i = 0; i < n; ++i) {
        uint8_t* e = data + i * g_fxStride;
        if (*(uint8_t**)e == f.comp && !memcmp(e + 4, f.bone, 8)) return e;
    }
    return nullptr;
}
static void FxRestore(FxEntry& f)
{
    if (!f.wrote) return;
    uint8_t* e = FxAttachment(f);
    if (!e) return;
    // A game reattach owns its new relative; never overwrite it with an old baseline.
    if (!memcmp(e + 0xC, f.tw, 12) && !memcmp(e + 0x18, f.rw, 12)) {
        memcpy(e + 0xC, f.t0, 12); memcpy(e + 0x18, f.r0, 12);
        InterlockedIncrement(&g_fxRestored);
    }
    f.wrote = false;
}
static bool FxWrite(FxEntry& f, const float* pos, const int32_t* rot)
{
    uint8_t* e = FxAttachment(f);
    if (!e) return false;
    const float* previousT = f.wrote ? f.tw : f.t0;
    const int32_t* previousR = f.wrote ? f.rw : f.r0;
    if (memcmp(e + 0xC, previousT, 12) || memcmp(e + 0x18, previousR, 12)) {
        f.wrote = false; return false; // the engine changed it during the calls
    }
    memcpy(e + 0xC, pos, 12); memcpy(e + 0x18, rot, 12);
    memcpy(f.tw, pos, 12); memcpy(f.rw, rot, 12); f.wrote = true;
    return true;
}

static bool FxL2W(uint8_t* comp, dvr::hf::Xform* out, bool light)
{
    const uint32_t off = light ? g_fxLightL2W : g_fxL2W;
    if (!off || !RangeReadable(comp + off, 64)) return false;
    const float* m = (const float*)(comp + off);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) out->r.m[r * 3 + c] = m[c * 4 + r];
    out->t[0] = m[12]; out->t[1] = m[13]; out->t[2] = m[14];
    for (float x : out->r.m) if (!MpFinite(x)) return false;
    for (float x : out->t) if (!MpFinite(x)) return false;
    return true;
}

static bool FxReadPublished(int hand, WaCommon* out, const char** why)
{
    AcquireSRWLockShared(&g_waCommonLock);
    *out = g_waCommon[hand];
    ReleaseSRWLockShared(&g_waCommonLock);
    if (!out->ok) { *why = "no hand correction published"; return false; }
    const uint32_t frame = (uint32_t)dvr::frame::count();
    if (frame - out->present > 4) { *why = "hand correction stale (hands not drawn)"; return false; }
    if (g_fxRequireNewPublication && (int32_t)(out->present - g_fxPublicationCutoff) <= 0) {
        *why = "waiting for a post-transition hand draw"; return false;
    }
    return true;
}

static bool FxWorldCorrection(const WaCommon& v, const WaComp& parent,
                              const WaComp* current, int count,
                              dvr::hf::Xform* W, float* travel, const char** why)
{
    const WaComp *ref = nullptr, *publishedParent = nullptr;
    for (int i = 0; i < v.componentCount && i < WA_MAX_COMP; ++i) {
        const auto& c = v.components[i];
        if (!c.ok) continue;
        if (c.isRef && !ref) ref = &c;
        if (FxSameId(c.id, parent.id) && c.isRef == parent.isRef && c.hand == parent.hand) publishedParent = &c;
    }
    if (!ref || !publishedParent) { *why = "parent or arm absent from published snapshot"; return false; }
    bool currentRef = false;
    for (int i = 0; i < count; ++i)
        if (current[i].ok && current[i].isRef && FxSameId(ref->id, current[i].id)) currentRef = true;
    if (!currentRef || ref->id.obj != ref->obj || parent.id.obj != parent.obj ||
        !FxLiveId(ref->id) || !FxLiveId(parent.id)) {
        *why = "parent or arm identity no longer current"; return false;
    }
    // Carry the published correction into this parent's LOCAL frame first. Reading the live
    // arm for every effect still mixes frames if the Heart has not had its own update yet.
    // Its bone natives and this live transform now name the same component at the same tick.
    dvr::hf::Xform liveParent, drawParent; float scale[3];
    const dvr::hf::Xform oldRef = { ref->R, { ref->t[0], ref->t[1], ref->t[2] } };
    const dvr::hf::Xform oldParent = { publishedParent->R,
        { publishedParent->t[0], publishedParent->t[1], publishedParent->t[2] } };
    if (!WaReadCompXform(parent.obj, &liveParent.r, liveParent.t, scale) ||
        !dvr::fx::parent_draw_reference(v.L_hand, oldRef, oldParent, &drawParent) ||
        !dvr::fx::world_correction(v.D, drawParent, liveParent, W)) {
        *why = "parent frame invalid"; return false;
    }
    float ds = 0;
    for (int k = 0; k < 3; ++k) { const float d = liveParent.t[k] - oldParent.t[k]; ds += d * d; }
    *travel = sqrtf(ds);
    return true;
}

static int FxHandFromBone(const char* bone)
{
    if (!bone) return -1;
    char b[64]; int n = 0;
    for (; bone[n] && n < 63; ++n) b[n] = (char)tolower((unsigned char)bone[n]);
    b[n] = 0;
    if (strstr(b, "left") || !strncmp(b, "l_", 2) || strstr(b, "_l_") || (n > 2 && !strcmp(b + n - 2, "_l"))) return 0;
    if (strstr(b, "right") || !strncmp(b, "r_", 2) || strstr(b, "_r_") || (n > 2 && !strcmp(b + n - 2, "_r"))) return 1;
    return -1;
}
static bool FxStrideFits(uint8_t* data, int num, uint32_t stride)
{
    if (!RangeReadable(data, num * stride)) return false;
    for (int i = 0; i < num; ++i) {
        uint8_t* e = data + i * stride;
        if (!IsLiveObject(*(uint8_t**)e)) return false;
        const float* s = (const float*)(e + 0x24);
        for (int k = 0; k < 3; ++k) if (!MpFinite(s[k]) || s[k] < 0.001f || s[k] > 1000.0f) return false;
    }
    return true;
}

struct FxBoneXform { float pos[3]; int32_t rot[3]; };
struct FxBoneParms { uint32_t nameIdx, nameNum; float inPos[3]; int32_t inRot[3]; float outPos[3]; int32_t outRot[3]; };
static const float kFxMaxMoveUU = 120.0f;
static bool FxBoneCall(bool toBone, const FxEntry& f, const float* pos, const int32_t* rot, FxBoneXform* out)
{
    auto& id = toBone ? g_fxFnTo : g_fxFnFrom;
    if (!FxLiveId(id)) {
        auto* fn = RainFindClassFunction("SkeletalMeshComponent", toBone ? "TransformToBoneSpace" : "TransformFromBoneSpace");
        if (!FxReadId(fn, &id)) return false;
    }
    if (!FxAttachment(f)) return false;
    FxBoneParms p{};
    p.nameIdx = f.bone[0]; p.nameNum = f.bone[1];
    memcpy(p.inPos, pos, 12); memcpy(p.inRot, rot, 12);
    const float sentinel = -1.0e30f; p.outPos[0] = sentinel;
    const bool oldReentry = g_peReentry;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(f.meshId.obj, id.obj, &p, NULL);
    g_peReentry = oldReentry;
    if (p.outPos[0] == sentinel) return false;
    for (float x : p.outPos) if (!MpFinite(x)) return false;
    memcpy(out->pos, p.outPos, 12); memcpy(out->rot, p.outRot, 12);
    return true;
}

static void FxFollowMesh(const WaComp& record, const dvr::hf::Xform* W, const bool* haveW, double now)
{
    uint8_t* mesh = record.obj;
    if (record.id.obj != mesh || !FxLiveId(record.id) || !RangeReadable(mesh + g_fxAttOff, 12)) return;
    uint8_t* data = *(uint8_t**)(mesh + g_fxAttOff);
    const int num = *(int*)(mesh + g_fxAttOff + 4);
    if (!data || num <= 0 || num > 64) return;
    if (!g_fxStride) {
        static const uint32_t kTry[] = { 0x30, 0x2C, 0x34, 0x3C, 0x38 };
        for (uint32_t stride : kTry) if (FxStrideFits(data, num, stride)) { g_fxStride = stride; break; }
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 4,
            "fx/follow: Attachments element size %s (0x%X) on %s, %d attachment(s)",
            g_fxStride ? "PROVED" : "NOT found - nothing will move", g_fxStride, ObjClassName(mesh), num);
        if (!g_fxStride) return;
    }
    if (!FxStrideFits(data, num, g_fxStride)) return;
    for (int i = 0; i < num; ++i) {
        // Revalidate the array after the previous attachment's ProcessEvent calls.
        if (!FxLiveId(record.id) || *(uint8_t**)(mesh + g_fxAttOff) != data ||
            *(int*)(mesh + g_fxAttOff + 4) != num || !RangeReadable(data, num * g_fxStride)) break;
        uint8_t* e = data + i * g_fxStride;
        uint8_t* c = *(uint8_t**)e;
        dvr::menukeep::Identity compId;
        if (!FxReadId(c, &compId)) continue;
        const char* cn = ObjClassName(c);
        const bool isLight = cn && strstr(cn, "LightComponent") != nullptr;
        if (!cn || (!strstr(cn, "ParticleSystemComponent") && !isLight)) continue;
        const char* bone = RealName(*(uint32_t*)(e + 4));
        const int hand = record.isRef ? FxHandFromBone(bone) : record.hand;
        FxEntry* f = FxFind(c);
        if (!f) {
            if (g_fxN >= (int)(sizeof(g_fx) / sizeof(g_fx[0]))) continue;
            f = &g_fx[g_fxN++]; *f = {};
        }
        if (!FxSameId(f->compId, compId) || !FxSameId(f->meshId, record.id) || memcmp(f->bone, e + 4, 8)) {
            *f = {}; f->comp = c; f->compId = compId; f->meshId = record.id; memcpy(f->bone, e + 4, 8);
            Log("fx/follow: tracking %s on %s bone '%s' (hand %s), current-frame attachment", cn, ObjClassName(mesh),
                bone ? bone : "?", hand == 0 ? "LEFT" : hand == 1 ? "RIGHT" : "UNKNOWN");
        }
        f->seenMs = now; f->hand = hand;
        if (!f->wrote || memcmp(e + 0xC, f->tw, 12) || memcmp(e + 0x18, f->rw, 12)) {
            memcpy(f->t0, e + 0xC, 12); memcpy(f->r0, e + 0x18, 12); f->wrote = false;
        }
        if (hand < 0 || hand > 1 || !haveW[hand]) { FxRestore(*f); continue; }
        FxBoneXform gameX;
        if (!FxBoneCall(false, *f, f->t0, f->r0, &gameX)) {
            g_fxWhy = "TransformFromBoneSpace refused"; FxRestore(*f); continue;
        }
        float tp[3], X[3], Y[3], Z[3], TX[3], TY[3], TZ[3];
        CtRotToAxes(gameX.rot, X, Y, Z);
        const auto& w = W[hand];
        for (int k = 0; k < 3; ++k)
            tp[k] = w.r.m[k * 3] * gameX.pos[0] + w.r.m[k * 3 + 1] * gameX.pos[1] + w.r.m[k * 3 + 2] * gameX.pos[2] + w.t[k];
        const float* ax[3] = { X, Y, Z }; float* tx[3] = { TX, TY, TZ };
        for (int a = 0; a < 3; ++a)
            for (int k = 0; k < 3; ++k)
                tx[a][k] = w.r.m[k * 3] * ax[a][0] + w.r.m[k * 3 + 1] * ax[a][1] + w.r.m[k * 3 + 2] * ax[a][2];
        if (!dvr::fireaim::normalize(TX) || !dvr::fireaim::normalize(TY) || !dvr::fireaim::normalize(TZ)) { FxRestore(*f); continue; }
        const float mv[3] = { tp[0] - gameX.pos[0], tp[1] - gameX.pos[1], tp[2] - gameX.pos[2] };
        const float move = sqrtf(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);
        if (!MpFinite(move) || move > kFxMaxMoveUU) {
            InterlockedIncrement(&g_fxRejected); g_fxWhy = "correction too large - rejected"; FxRestore(*f); continue;
        }
        int32_t trot[3]; CtAxesToRot(TX, TY, TZ, trot);
        FxBoneXform rel;
        if (!FxBoneCall(true, *f, tp, trot, &rel)) { g_fxWhy = "TransformToBoneSpace refused"; FxRestore(*f); continue; }
        if (!FxWrite(*f, rel.pos, rel.rot)) { g_fxWhy = "attachment changed during bone calls"; continue; }
        f->lastMove = move; g_fxWhy = "driving in current parent frame";
        InterlockedIncrement(&g_fxDriven);
    }
}

// ---- discovery: every particle system and light the player owns (VR-182, second run) --------
// Build 665 tracked nothing (the filter bug below), and the headset report adds what the scripts
// could not: the Heart's stray effect is a floating ball of LIGHT, and Possession shows two
// effects, a light and a particle system, neither on the hand. Some of these may not be mesh
// attachments at all (a component on the item's or the power's actor, or on the pawn). This
// finds them wherever they live: an incremental GObjects pass (8192 objects a tick, so it never
// stalls) keeps every live ParticleSystemComponent or LightComponent whose Owner is the pawn or an
// actor the pawn owns. Each is logged once when found - class, name, template, owner, and whether
// a tracked mesh carries it in Attachments - and every three seconds with where it sits in the view.
struct FxSeen { uint8_t* comp; };
static FxSeen g_fxSeen[48]; static int g_fxSeenN = 0;
static uint32_t g_fxScanAt = 0;

// Class pointer -> is it a particle system or a light. A GObjects entry is a live UObject, so its
// class pointer at +kClassOff is read raw; only a class not seen before pays ObjClassNames two
// VirtualQuery calls. There are a few thousand classes, so the cache fills once and then the scan
// is a pointer read and a hash probe per object.
static uint8_t* g_fxClsKey[8192]; static int8_t g_fxClsVal[8192];
static int FxClassKind(uint8_t* cls)                  // 0 no, 1 particle system, 2 light
{
    uint32_t h = (uint32_t)(((uintptr_t)cls >> 3) * 2654435761u) & 8191u;
    for (int probe = 0; probe < 16; ++probe, h = (h + 1) & 8191u) {
        if (g_fxClsKey[h] == cls) return g_fxClsVal[h];
        if (!g_fxClsKey[h]) {
            const char* cn = (RangeReadable(cls, kNameOff + 8)) ? RealName(*(uint32_t*)(cls + kNameOff)) : nullptr;
            const int kind = !cn ? 0 : strstr(cn, "ParticleSystemComponent") ? 1 : strstr(cn, "LightComponent") ? 2 : 0;
            g_fxClsKey[h] = cls; g_fxClsVal[h] = (int8_t)kind;
            return kind;
        }
    }
    return 0;                                         // table crowded here: skip, never stall
}

static bool FxInTrackedAttachments(uint8_t* comp)
{
    for (int i = 0; i < g_fxN; ++i) if (g_fx[i].comp == comp) return true;
    return false;
}

static void FxDiscoverTick(double now)
{
    static uint32_t oOwner = 0, oActOwner = 0, oTemplate = 0;
    if (!oOwner) {
        oOwner = RflOffsetOf("ActorComponent", "Owner");
        oActOwner = RflOffsetOf("Actor", "Owner");
        oTemplate = RflOffsetOf("ParticleSystemComponent", "Template");
        if (!oOwner) return;
    }
    // Build 666 ran this EVERY tick over 8192 objects with a RangeReadable (a VirtualQuery system
    // call) on each, in the main menu too: the game fell to 0-5 fps. Now: gameplay only, 2048
    // objects every 100 ms, and the class NAME is tested first (one pointer chase, safe on any
    // UObject) so the checked reads happen only for the few particle and light components.
    if (g_mainMenu || g_inMenu || g_menuOpen || !CylTruthLive()) return;
    static double nextChunk = 0;
    if (now < nextChunk) return;
    nextChunk = now + 100;
    uint8_t* pawn = g_pePawn;
    if (!pawn || !RangeReadable((void*)kGObjHdr, 12)) return;
    void** objs = *(void***)kGObjHdr;
    const uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return;
    if (g_fxScanAt >= onum) g_fxScanAt = 0;
    const uint32_t end = g_fxScanAt + 2048 < onum ? g_fxScanAt + 2048 : onum;
    if (!RangeReadable(objs + g_fxScanAt, (end - g_fxScanAt) * sizeof(void*))) { g_fxScanAt = 0; return; }
    for (uint32_t i = g_fxScanAt; i < end; ++i) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3)) continue;
        uint8_t* cls = *(uint8_t**)(o + kClassOff);        // GObjects entries are live UObjects
        if (!cls || ((uintptr_t)cls & 3) || !FxClassKind(cls)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || !RangeReadable(o + oOwner, 4)) continue;
        uint8_t* own = *(uint8_t**)(o + oOwner);
        if (!own || ((uintptr_t)own & 3)) continue;
        bool mine = own == pawn;
        if (!mine && oActOwner && RangeReadable(own + oActOwner, 4)) mine = *(uint8_t**)(own + oActOwner) == pawn;
        if (!mine) continue;
        bool known = false;
        for (int k = 0; k < g_fxSeenN; ++k) if (g_fxSeen[k].comp == o) { known = true; break; }
        if (known || g_fxSeenN >= (int)(sizeof(g_fxSeen) / sizeof(g_fxSeen[0])) || !IsLiveObject(o)) continue;
        const char* nm = RealName(*(uint32_t*)(o + kNameOff));
        uint8_t* tpl = (oTemplate && strstr(cn, "Particle") && RangeReadable(o + oTemplate, 4)) ? *(uint8_t**)(o + oTemplate) : nullptr;
        const char* tn = (tpl && IsLiveObject(tpl)) ? RealName(*(uint32_t*)(tpl + kNameOff)) : "-";
        g_fxSeen[g_fxSeenN++].comp = o;
        Log("fx/find: %s '%s' template '%s' owned by %s%s - %s", cn, nm ? nm : "?", tn ? tn : "-", ObjClassName(own),
            own == pawn ? " (the pawn)" : " (owned by the pawn)",
            FxInTrackedAttachments(o) ? "a TRACKED mesh attachment" : "NOT in any tracked mesh's Attachments");
    }
    g_fxScanAt = end >= onum ? 0 : end;
    static double nextPos = 0;
    if (now < nextPos || !g_fxSeenN) return;
    nextPos = now + 3000;
    float cam[3]; if (!CarryGameAnchor(cam)) return;
    const float cy = cosf(g_viewYawRad), sy = sinf(g_viewYawRad), cp = cosf(g_viewPitchRad), sp = sinf(g_viewPitchRad);
    for (int k = 0; k < g_fxSeenN; ) {
        uint8_t* o = g_fxSeen[k].comp;
        if (!IsLiveObject(o)) { g_fxSeen[k] = g_fxSeen[--g_fxSeenN]; continue; }
        const char* cn = ObjClassName(o);
        dvr::hf::Xform C;
        if (FxL2W(o, &C, cn && strstr(cn, "LightComponent") != nullptr)) {
            const float r[3] = { C.t[0] - cam[0], C.t[1] - cam[1], C.t[2] - cam[2] };
            DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 300,
                "fx/find:   %s '%s' at %.0f fwd %.0f right %.0f up uu in the view%s", cn, RealName(*(uint32_t*)(o + kNameOff)),
                r[0] * cp * cy + r[1] * cp * sy + r[2] * sp, -r[0] * sy + r[1] * cy,
                -r[0] * sp * cy - r[1] * sp * sy + r[2] * cp, FxInTrackedAttachments(o) ? " (tracked)" : "");
        }
        ++k;
    }
}


// Rebuild after menus/loads before checking identities. The pointer table captured in the
// main menu cannot establish liveness for components created by the loaded level.
static bool FxPrepareLifetime()
{
    static bool wasActive = false, wasMenu = false;
    static LONG loadAt = 0;
    static dvr::menukeep::Identity pawnId;
    const bool active = !g_mainMenu && g_pePawn && CylTruthLive();
    const bool menu = g_inMenu || g_menuOpen;
    const uint32_t frame = (uint32_t)dvr::frame::count();
    if (!active) {
        if (wasActive && BuildLiveSet()) for (int i = 0; i < g_fxN; ++i) FxRestore(g_fx[i]);
        g_fxN = 0; wasActive = false;
        g_fxPublicationCutoff = frame; g_fxRequireNewPublication = true;
        return false;
    }
    bool reset = !wasActive || loadAt != g_mkLoadEvents || pawnId.obj != g_pePawn;
    if (!(reset || menu != wasMenu ? BuildLiveSet() : RefreshLiveSet(1000))) return false;
    dvr::menukeep::Identity currentPawn;
    if (!FxReadId(g_pePawn, &currentPawn)) return false;
    if (!FxSameId(pawnId, currentPawn)) reset = true;
    if (reset) {
        for (int i = 0; i < g_fxN; ++i) FxRestore(g_fx[i]);
        g_fxN = 0; g_fxSeenN = 0; g_fxScanAt = 0;
        memset(g_fxClsKey, 0, sizeof(g_fxClsKey)); memset(g_fxClsVal, 0, sizeof(g_fxClsVal));
        g_fxFnFrom = {}; g_fxFnTo = {};
        g_fxPublicationCutoff = frame; g_fxRequireNewPublication = true;
        Log("fx/follow: attachment lifetime revalidated; waiting for a fresh hand draw after frame %u", frame);
    }
    pawnId = currentPawn; loadAt = g_mkLoadEvents; wasMenu = menu; wasActive = true;
    return true;
}

static void FxFollowTickBody();
// The script tick runs on EVERY ProcessEvent dispatch (thousands a second), and the hook has no
// re-entry guard (process_event.cpp's own header says g_peReentry is not read there). Build 668
// called TransformFromBoneSpace through ProcessEvent from inside this tick: the call re-entered
// the hook, the hook ran this tick again, which called the engine again - a stack overflow
// (0xC00000FD in d3d9.dll) on the first save load. So: never nested, and once per frame.
static void FxFollowTick()
{
    static bool inside = false;
    if (inside) return;
    static uint32_t lastFrame = 0xffffffffu;
    const uint32_t frame = (uint32_t)dvr::frame::count();
    if (frame == lastFrame) return;
    lastFrame = frame;
    inside = true;
    FxFollowTickBody();
    inside = false;
}
static void FxFollowTickBody()
{
    {   // VR-183: which hands hold an item - the hand placement keeps the calibrated frame for those
        bool held[2] = { false, false };
        AcquireSRWLockShared(&g_waCompLock);
        for (int i = 0; i < g_waCompN; ++i)
            if (g_waComp[i].ok && g_waComp[i].isMember && g_waComp[i].hand >= 0 && g_waComp[i].hand <= 1) held[g_waComp[i].hand] = true;
        ReleaseSRWLockShared(&g_waCompLock);
        // VR-224 Index tuning: the empty left hand (powers, Blink, the Heart) is re-posed in
        // the palm draw, so its aim ray is lifted to match; the pistol and crossbow keep theirs.
        // 23 deg measured in the headset on an Index rig: the Blink dot sat that far below the
        // fingers. Paired with the shim's -21 deg hold pitch; off, both are zero.
        dvr::vr::input_set_aim_pitch_extra(0, (g_indexTuning && !held[0]) ? 23.0f : 0.0f);
        for (int h = 0; h < 2; ++h)
            if (held[h] != g_mpItemInHand[h]) {
                g_mpItemInHand[h] = held[h];
                Log("hands/anchor: %s hand %s - placed from the %s", h ? "RIGHT" : "LEFT",
                    held[h] ? "holds an item" : "is empty", held[h] ? "calibrated item frame (as before)" : "WRIST, finger animation cannot swing it");
            }
    }
    if (!RflNamesReady() || !FxPrepareLifetime()) return;
    if (!g_fxAttOff) {
        static double nextTry = 0; const double t = MaimNowMs();
        if (t < nextTry) return;
        nextTry = t + 5000;
        g_fxAttOff = RflOffsetOf("SkeletalMeshComponent", "Attachments");
        if (!g_fxAttOff) return;
        g_fxLightL2W = RflOffsetOf("LightComponent", "LightToWorld");
        Log("fx/follow: SkeletalMeshComponent.Attachments +0x%X; component LocalToWorld +0x%X; LightComponent.LightToWorld +0x%X",
            g_fxAttOff, g_fxL2W, g_fxLightL2W);
    }
    const double now = MaimNowMs();
    WaCommon published[2]; bool available[2] = {}, anyCorrection[2] = {};
    const char* why[2] = { "", "" };
    for (int h = 0; h < 2; ++h) {
        available[h] = FxReadPublished(h, &published[h], &why[h]);
        g_fxParentTravel[h] = 0;
    }
    // The meshes the hands path corrects: the snapshot the weapon path keeps.
    WaComp comps[WA_MAX_COMP]; int n = 0;
    AcquireSRWLockShared(&g_waCompLock);
    n = g_waCompN; memcpy(comps, g_waComp, sizeof(comps));
    ReleaseSRWLockShared(&g_waCompLock);
    for (int i = 0; i < n; ++i) {
        if (!comps[i].ok || !comps[i].obj || !FxLiveId(comps[i].id)) continue;
        const char* cn = ObjClassName(comps[i].obj);
        // Build 665: this said "SkeletalMeshComponent" and skipped every mesh the game uses - they are
        // subclasses (DishonoredItemSkeletalComponent, the player's skeletal component) - so nothing was
        // ever tracked. Attachments is declared on the base, so any skeletal subclass carries it.
        if (!cn || !strstr(cn, "Skeletal")) continue;
        dvr::hf::Xform W[2]; bool haveW[2] = {};
        for (int h = 0; h < 2; ++h) {
            if (!available[h] || (!comps[i].isRef && comps[i].hand != h)) continue;
            float travel = 0;
            haveW[h] = FxWorldCorrection(published[h], comps[i], comps, n, &W[h], &travel, &why[h]);
            anyCorrection[h] = anyCorrection[h] || haveW[h];
            if (travel > g_fxParentTravel[h]) g_fxParentTravel[h] = travel;
        }
        FxFollowMesh(comps[i], W, haveW, now);
    }
    // Entries not seen for a while are gone (unequipped, destroyed): forget them.
    for (int i = 0; i < g_fxN; ) {
        if (now - g_fx[i].seenMs > 3000) { FxRestore(g_fx[i]); g_fx[i] = g_fx[--g_fxN]; continue; }
        ++i;
    }
    FxDiscoverTick(now);
    static double nextLog = 0;
    if (g_fxN && now >= nextLog) {
        nextLog = now + 2000;
        float mx = 0; for (int i = 0; i < g_fxN; ++i) if (g_fx[i].lastMove > mx) mx = g_fx[i].lastMove;
        Log("fx/follow: %d effect(s) tracked, %ld corrections, %ld restored, %ld rejected as too large (limit %.0f uu); largest move %.1f uu (last: %s). Left %s, right %s; current parent travel %.3f/%.3f uu",
            g_fxN, (long)g_fxDriven, (long)g_fxRestored, (long)g_fxRejected, kFxMaxMoveUU, mx, g_fxWhy, anyCorrection[0] ? "live" : why[0], anyCorrection[1] ? "live" : why[1], g_fxParentTravel[0], g_fxParentTravel[1]);
    }
}

#undef DVR_CAT
