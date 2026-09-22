// game/dishonored/hands/fx_follow.cpp - VR-182: EFFECTS ATTACHED TO THE HANDS FOLLOW THE DRAWN HANDS.
//
// The hands and held items are placed by a RENDER-ONLY correction (VR-33-HANDS-AND-WEAPONS
// section 1): one rigid D composed onto every bone palette at draw time, while the game's bones
// stay where the animation put them. Anything the game ATTACHES to those meshes therefore sits at
// the game's hand, not the drawn one: the Heart's inner glow (DisGadget_Heart.m_pGlowFX on
// DisTweaks_Heart.m_GlowFXSocket), Blink's hand effect (DishonoredActivePowerComponent_Blink.
// m_pMeshPS) and Possession's hand-cast effect (DisTweaks_Possess.m_pHandCastParticle on
// m_HandCastParticleAttachSocket). All are ParticleSystemComponents in a SkeletalMeshComponent's
// Attachments array; the socket names live in the content packages, so nothing here names them.
//
// The correction is already published per hand for the weapon path (WaPublishCommon): D in the
// draw's camera-relative space, the reference (arm) mesh's draw transform L_hand, and the script
// lane's snapshot of each mesh component's NATIVE transform. So in world terms
//     W = inverse(br) * D * br,   br = bridge(native arm, drawn arm)   (native -> draw space)
// and an attached component whose world transform is C = S * Rel (S the socket) should be at
// W * S * Rel0. The engine recomputes C from the attachment's RelativeLocation/RelativeRotation
// every update (the item meshes force it: bForceUpdateAttachmentsInTick), so the lever is the
// attachment's relative transform:
//     S       = C * inverse(RelWritten)        (the socket, recovered from what the engine built)
//     RelNew  = inverse(S) * W * S * Rel0
// Rel0 is captured the first time a component is seen and put back when no fresh correction is
// available, so a frame without hands leaves the game's own placement. Script lane only.

#define DVR_CAT ::dvr::log::Cat::hands

struct FxEntry {
    uint8_t* comp;             // the attached component (identity; IsLiveObject before every use)
    float t0[3]; int32_t r0[3];      // the game's own relative transform
    float tw[3]; int32_t rw[3];      // what we wrote last
    bool wrote;
    double seenMs;
    int hand;
    float lastMove;            // uu, how far the last correction moved it
};
static FxEntry g_fx[24];
static int g_fxN = 0;
static uint32_t g_fxStride = 0;                // Attachments element size, found at runtime
static uint32_t g_fxAttOff = 0, g_fxL2W = kWaComponentLocalToWorld, g_fxLightL2W = 0;
static volatile LONG g_fxDriven = 0, g_fxRestored = 0;
static const char* volatile g_fxWhy = "not asked yet";

static FxEntry* FxFind(uint8_t* comp)
{
    for (int i = 0; i < g_fxN; ++i) if (g_fx[i].comp == comp) return &g_fx[i];
    return nullptr;
}

// A component's native world transform, column form (y = r*x + t), scale kept in the columns.
static bool FxL2W(uint8_t* comp, dvr::hf::Xform* out, bool light)
{
    const uint32_t off = light ? g_fxLightL2W : g_fxL2W;
    if (!off || !RangeReadable(comp + off, 64)) return false;
    const float* m = (const float*)(comp + off);    // native rows: X, Y, Z basis, translation
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) out->r.m[r * 3 + c] = m[c * 4 + r];
    out->t[0] = m[12]; out->t[1] = m[13]; out->t[2] = m[14];
    for (int i = 0; i < 9; ++i) if (!MpFinite(out->r.m[i])) return false;
    for (int i = 0; i < 3; ++i) if (!MpFinite(out->t[i])) return false;
    return true;
}

static dvr::hf::Xform FxRel(const float* t, const int32_t* r, const float* s)
{
    float X[3], Y[3], Z[3]; CtRotToAxes(r, X, Y, Z);
    dvr::hf::Xform x;
    for (int i = 0; i < 3; ++i) {
        x.r.m[i * 3 + 0] = X[i] * s[0]; x.r.m[i * 3 + 1] = Y[i] * s[1]; x.r.m[i * 3 + 2] = Z[i] * s[2];
        x.t[i] = t[i];
    }
    return x;
}

// The world-space correction for one hand, from the weapon path's published data.
static bool FxWorldCorrection(int hand, dvr::hf::Xform* W, const char** why)
{
    WaCommon v;
    for (int tries = 0; tries < 2; ++tries) {             // the render thread writes it whole
        const uint32_t p0 = g_waCommon[hand].present;
        v = g_waCommon[hand];
        if (v.present == p0 && g_waCommon[hand].present == p0) break;
    }
    if (!v.ok) { *why = "no hand correction published"; return false; }
    const uint32_t now = (uint32_t)dvr::frame::count();
    if (now - v.present > 4) { *why = "hand correction stale (hands not drawn)"; return false; }
    const WaComp* ref = nullptr;
    for (int i = 0; i < v.componentCount; ++i)
        if (v.components[i].ok && v.components[i].isRef) { ref = &v.components[i]; break; }
    if (!ref) { *why = "no arm mesh in the snapshot"; return false; }
    dvr::hf::Xform nr = { ref->R, { ref->t[0], ref->t[1], ref->t[2] } }, br, ibr;
    if (!dvr::wf::bridge(nr, v.L_hand, &br) || !dvr::wf::inverse(br, &ibr)) { *why = "bridge degenerate"; return false; }
    *W = dvr::hf::xform_mul(dvr::hf::xform_mul(ibr, v.D), br);
    for (int i = 0; i < 9; ++i) if (!MpFinite(W->r.m[i])) { *why = "correction not finite"; return false; }
    for (int i = 0; i < 3; ++i) if (!MpFinite(W->t[i])) { *why = "correction not finite"; return false; }
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

// The Attachments element size, proved on a live array: every element must name a live
// component and carry a sane relative scale. Tried once per array until one fits.
static bool FxStrideFits(uint8_t* data, int num, uint32_t stride)
{
    if (!RangeReadable(data, num * stride)) return false;
    for (int i = 0; i < num; ++i) {
        uint8_t* e = data + i * stride;
        uint8_t* c = *(uint8_t**)e;
        if (!c || !IsLiveObject(c)) return false;
        const float* s = (const float*)(e + 0x24);
        for (int k = 0; k < 3; ++k) if (!MpFinite(s[k]) || s[k] < 0.001f || s[k] > 1000.0f) return false;
    }
    return true;
}

static void FxFollowMesh(uint8_t* mesh, int meshHand, const dvr::hf::Xform* W, const bool* haveW, double now)
{
    if (!mesh || !IsLiveObject(mesh) || !RangeReadable(mesh + g_fxAttOff, 12)) return;
    uint8_t* data = *(uint8_t**)(mesh + g_fxAttOff);
    const int num = *(int*)(mesh + g_fxAttOff + 4);
    if (!data || num <= 0 || num > 64) return;
    if (!g_fxStride) {
        static const uint32_t kTry[] = { 0x30, 0x2C, 0x34, 0x3C, 0x38 };
        for (uint32_t s : kTry) if (FxStrideFits(data, num, s)) { g_fxStride = s; break; }
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 4,
            "fx/follow: Attachments element size %s (0x%X) on %s, %d attachment(s)",
            g_fxStride ? "PROVED" : "NOT found - nothing will move", g_fxStride, ObjClassName(mesh), num);
        if (!g_fxStride) return;
    }
    if (!FxStrideFits(data, num, g_fxStride)) return;
    for (int i = 0; i < num; ++i) {
        uint8_t* e = data + i * g_fxStride;
        uint8_t* c = *(uint8_t**)e;
        const char* cn = ObjClassName(c);
        const bool isLight = cn && strstr(cn, "LightComponent") != nullptr;
        if (!cn || (!strstr(cn, "ParticleSystemComponent") && !isLight)) {
            DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 24, "fx/follow: %s on %s bone %s - neither a particle system nor a light, left alone",
                cn ? cn : "?", ObjClassName(mesh), RealName(*(uint32_t*)(e + 4)));
            continue;
        }
        const char* bone = RealName(*(uint32_t*)(e + 4));
        int hand = meshHand >= 0 ? meshHand : FxHandFromBone(bone);
        FxEntry* f = FxFind(c);
        if (!f) {
            if (g_fxN >= (int)(sizeof(g_fx) / sizeof(g_fx[0]))) continue;
            f = &g_fx[g_fxN++];
            memset(f, 0, sizeof(*f));
            f->comp = c;
            memcpy(f->t0, e + 0xC, 12); memcpy(f->r0, e + 0x18, 12);
            Log("fx/follow: tracking %s on %s bone '%s' (hand %s) - relative %.1f %.1f %.1f", cn, ObjClassName(mesh),
                bone ? bone : "?", hand == 0 ? "LEFT" : hand == 1 ? "RIGHT" : "UNKNOWN", f->t0[0], f->t0[1], f->t0[2]);
        }
        f->seenMs = now; f->hand = hand;
        float* relT = (float*)(e + 0xC); int32_t* relR = (int32_t*)(e + 0x18); const float* relS = (const float*)(e + 0x24);
        // The game set it again (a re-attach): its value is the new baseline.
        if (f->wrote && (memcmp(relT, f->tw, 12) || memcmp(relR, f->rw, 12))) {
            memcpy(f->t0, relT, 12); memcpy(f->r0, relR, 12); f->wrote = false;
        }
        const bool ok = hand >= 0 && hand <= 1 && haveW[hand];
        if (!ok) {
            if (f->wrote) { memcpy(relT, f->t0, 12); memcpy(relR, f->r0, 12); f->wrote = false; InterlockedIncrement(&g_fxRestored); }
            continue;
        }
        dvr::hf::Xform C, invRelNow, invS;
        if (!FxL2W(c, &C, isLight)) continue;
        const dvr::hf::Xform relNow = FxRel(relT, relR, relS), rel0 = FxRel(f->t0, f->r0, relS);
        if (!dvr::wf::inverse(relNow, &invRelNow)) continue;
        const dvr::hf::Xform S = dvr::hf::xform_mul(C, invRelNow);
        if (!dvr::wf::inverse(S, &invS)) continue;
        const dvr::hf::Xform target = dvr::hf::xform_mul(W[hand], dvr::hf::xform_mul(S, rel0));
        const dvr::hf::Xform relNew = dvr::hf::xform_mul(invS, target);
        float X[3], Y[3], Z[3];
        for (int k = 0; k < 3; ++k) { X[k] = relNew.r.m[k * 3 + 0]; Y[k] = relNew.r.m[k * 3 + 1]; Z[k] = relNew.r.m[k * 3 + 2]; }
        if (!dvr::fireaim::normalize(X) || !dvr::fireaim::normalize(Y) || !dvr::fireaim::normalize(Z)) continue;
        bool finite = true;
        for (int k = 0; k < 3; ++k) finite = finite && MpFinite(relNew.t[k]);
        if (!finite) continue;
        int32_t nr[3]; CtAxesToRot(X, Y, Z, nr);
        const float mv[3] = { target.t[0] - C.t[0], target.t[1] - C.t[1], target.t[2] - C.t[2] };
        f->lastMove = sqrtf(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);
        memcpy(relT, relNew.t, 12); memcpy(relR, nr, 12);
        memcpy(f->tw, relNew.t, 12); memcpy(f->rw, nr, 12); f->wrote = true;
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
    uint8_t* pawn = g_pePawn;
    if (!pawn || !RangeReadable((void*)kGObjHdr, 12)) return;
    void** objs = *(void***)kGObjHdr;
    const uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return;
    if (g_fxScanAt >= onum) g_fxScanAt = 0;
    const uint32_t end = g_fxScanAt + 8192 < onum ? g_fxScanAt + 8192 : onum;
    if (!RangeReadable(objs + g_fxScanAt, (end - g_fxScanAt) * sizeof(void*))) { g_fxScanAt = 0; return; }
    for (uint32_t i = g_fxScanAt; i < end; ++i) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, oOwner + 4)) continue;
        uint8_t* own = *(uint8_t**)(o + oOwner);
        if (!own || ((uintptr_t)own & 3)) continue;
        bool mine = own == pawn;
        if (!mine && oActOwner && RangeReadable(own + oActOwner, 4)) mine = *(uint8_t**)(own + oActOwner) == pawn;
        if (!mine) continue;
        const char* cn = ObjClassName(o);
        if (!cn || (!strstr(cn, "ParticleSystemComponent") && !strstr(cn, "LightComponent"))) continue;
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

static void FxFollowTick()
{
    if (!RflNamesReady()) return;
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
    dvr::hf::Xform W[2]; bool haveW[2] = { false, false };
    const char* why[2] = { "", "" };
    for (int h = 0; h < 2; ++h) haveW[h] = FxWorldCorrection(h, &W[h], &why[h]);
    // The meshes the hands path corrects: the snapshot the weapon path keeps.
    WaComp comps[WA_MAX_COMP]; int n = 0;
    AcquireSRWLockShared(&g_waCompLock);
    n = g_waCompN; memcpy(comps, g_waComp, sizeof(comps));
    ReleaseSRWLockShared(&g_waCompLock);
    for (int i = 0; i < n; ++i) {
        if (!comps[i].ok || !comps[i].obj) continue;
        const char* cn = ObjClassName(comps[i].obj);
        // Build 665: this said "SkeletalMeshComponent" and skipped every mesh the game uses - they are
        // subclasses (DishonoredItemSkeletalComponent, the player's skeletal component) - so nothing was
        // ever tracked. Attachments is declared on the base, so any skeletal subclass carries it.
        if (!cn || !strstr(cn, "Skeletal")) continue;
        FxFollowMesh(comps[i].obj, comps[i].isRef ? -1 : comps[i].hand, W, haveW, now);
    }
    // Entries not seen for a while are gone (unequipped, destroyed): forget them.
    for (int i = 0; i < g_fxN; ) {
        if (now - g_fx[i].seenMs > 3000) { g_fx[i] = g_fx[--g_fxN]; continue; }
        ++i;
    }
    FxDiscoverTick(now);
    static double nextLog = 0;
    if (g_fxN && now >= nextLog) {
        nextLog = now + 2000;
        float mx = 0; for (int i = 0; i < g_fxN; ++i) if (g_fx[i].lastMove > mx) mx = g_fx[i].lastMove;
        Log("fx/follow: %d effect(s) tracked, %ld corrections, %ld restored; largest move %.1f uu. Left %s, right %s",
            g_fxN, (long)g_fxDriven, (long)g_fxRestored, mx, haveW[0] ? "live" : why[0], haveW[1] ? "live" : why[1]);
    }
}

#undef DVR_CAT
