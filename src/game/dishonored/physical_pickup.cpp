// game/dishonored/physical_pickup.cpp - loot picked up by reaching for it.
//
// Reach a hand to a lootable item and squeeze that hand's grip: the item is picked up. The
// Interact button keeps working exactly as before, at any range the game allows.
//
// Nothing here picks anything up by itself, and no engine field is written. Three small parts:
//
//   1. THE LIST. Every actor whose class derives from DisPickup_Base (coins, elixirs, keys, bone
//      charms, ammunition, notes), or is a DisProjectile_Arrow (a bolt to recover), found by an
//      INCREMENTAL pass over GObjects: a couple of thousand slots a frame, so a full sweep takes
//      well under a second and no frame pays for a whole walk. An entry is used only while its
//      GObjects slot still holds the same pointer with the same class.
//   2. THE TARGET. Each frame the nearest listed item within reach of either hand becomes the
//      target, and interact_aim.cpp's two bridges hand the ENGINE's own interaction trace a ray
//      from the head to that item instead of the pointing ray. The engine still traces,
//      validates, highlights and prompts: the highlight is the game's, and an item the game
//      would refuse stays refused. A target the engine does not focus within a quarter second
//      is dropped for a few seconds (a looted item that is still an actor, something in the way).
//   3. THE GRIP. While the engine's focused actor IS the target and a hand is within reach, a
//      press of that hand's PHYSICAL grip is swallowed (the power wheel or the block bound to
//      it does not fire) and Interact is pressed for a moment instead (pad_bridge.cpp).
//
// [Aim] PhysicalPickup (default 1), PhysicalPickupReachCm (default 30). F10 > Aim. Seam: `pickup`.
// Script lane for 1 and 2 (the interaction bridges run on the same game thread); the pad
// bridge reads two atomics.

#define DVR_CAT ::dvr::log::Cat::script

static std::atomic<bool>     g_ppOn{true};
static std::atomic<float>    g_ppReachM{0.30f};
static std::atomic<uint32_t> g_ppReadyMask{0};        // bit h: hand h may pick the target up now
static std::atomic<uint64_t> g_ppReadyMs{0};          // when the mask was last written
static std::atomic<uint32_t> g_ppFired{0}, g_ppSwallowed{0};

struct PpEntry { uint8_t* obj; uint32_t idx; uint8_t* cls; };
static std::vector<PpEntry> g_ppList, g_ppBuild;
static std::unordered_map<uint8_t*, uint8_t> g_ppClassVerdict;   // class pointer -> 1 lootable, 0 not
static uint32_t g_ppCursor = 0, g_ppSweeps = 0;
static double   g_ppSweepStartMs = 0, g_ppLastSweepMs = 0;

// The target (game thread only).
static uint8_t* g_ppTarget = NULL;
static uint32_t g_ppTargetIdx = 0;
static float    g_ppTargetLoc[3] = {};
static int      g_ppTargetHand = -1;
static double   g_ppTargetSinceMs = 0, g_ppFocusedLastMs = 0;
static bool     g_ppTargetFocused = false;
static uint8_t* g_ppBlock = NULL; static double g_ppBlockUntilMs = 0, g_ppBlockForMs = 0;   // doubles while the same item keeps refusing
static volatile LONG g_ppRayDriven = 0;

static bool PickupEnabled() { return g_ppOn.load(); }

static bool PpGameplay()
{
    return g_ppOn.load() && !g_gamepadOnly && CylTruthLive() && !g_menuOpen && !g_inMenu && !g_mainMenu && !g_cineNow;
}

// Does this class derive from a lootable base? Walks the SuperField chain once per class.
static bool PpClassLootable(uint8_t* cls)
{
    auto it = g_ppClassVerdict.find(cls);
    if (it != g_ppClassVerdict.end()) return it->second != 0;
    bool yes = false;
    uint8_t* c = cls;
    for (int depth = 0; c && depth < 32; ++depth) {
        if (((uintptr_t)c & 3) || !RangeReadable(c, kSuperFieldOff + 4)) break;
        const char* n = RealName(*(uint32_t*)(c + kNameOff));
        if (n && (!strcmp(n, "DisPickup_Base") || !strcmp(n, "DisProjectile_Arrow"))) { yes = true; break; }
        c = *(uint8_t**)(c + kSuperFieldOff);
    }
    if (g_ppClassVerdict.size() < 20000) g_ppClassVerdict.emplace(cls, (uint8_t)(yes ? 1 : 0));
    return yes;
}

// A slice of the incremental sweep. `budget` GObjects slots a call.
static void PpSweep(uint32_t budget, double nowMs)
{
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || num < 1000 || num > 4000000) return;
    if (g_ppCursor == 0) { g_ppBuild.clear(); g_ppSweepStartMs = nowMs; }
    ::dvr::mem::RegionMemo rt, ro;
    uint32_t i = g_ppCursor;
    const uint32_t end = (num - i > budget) ? i + budget : num;
    for (; i < end; ++i) {
        if (!rt.ok(objs + i, sizeof(void*))) { i = num; break; }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !ro.ok(o, kClassOff + 4)) continue;
        uint8_t* cls = *(uint8_t**)(o + kClassOff);
        if (!cls || !PpClassLootable(cls)) continue;
        if (g_ppBuild.size() < 8192) g_ppBuild.push_back({o, i, cls});
    }
    g_ppCursor = i;
    if (g_ppCursor >= num) {
        g_ppCursor = 0;
        g_ppList.swap(g_ppBuild);
        g_ppLastSweepMs = nowMs - g_ppSweepStartMs;
        // A class pointer can be reused by another class after a level change; the verdicts are
        // cheap to rebuild, so they do not outlive a few dozen sweeps.
        if ((g_ppSweeps & 63u) == 63u) g_ppClassVerdict.clear();
        if (++g_ppSweeps == 1)
            Log("pickup: first sweep done - %u lootable actor(s) among %u objects in %.0f ms of game time "
                "(%u slots a frame; class verdicts cached for %u classes)",
                (unsigned)g_ppList.size(), num, g_ppLastSweepMs, budget, (unsigned)g_ppClassVerdict.size());
    }
}

// A hand's position in game world units: the grip pose, scaled about the head the way the drawn
// hand is, through the same head-to-world mapping the published aim ray uses.
static bool PpHandWorld(int hand, const float camera[3], float out[3])
{
#if DVR_WITH_OPENXR
    const uint64_t now = GetTickCount64();
    const dvr::vr::HandAimSample s = dvr::vr::input_hand_aim_sample(hand);
    dvr::vr::HeadPose head;
    if (!s.gripValid || !dvr::vr::peek_head_pose(head)) return false;
    dvr::aim::FireFrame f;
    f.headValid = true;
    f.headPos[0] = head.px; f.headPos[1] = head.py; f.headPos[2] = head.pz;
    f.headQuat[0] = head.qx; f.headQuat[1] = head.qy; f.headQuat[2] = head.qz; f.headQuat[3] = head.qw;
    f.distanceM = 1.0f;
    float pos[3] = { s.gripPos[0], s.gripPos[1], s.gripPos[2] };
    const dvr::hands::TrimSnapshot cal = dvr::hands::trim_snapshot(hand);
    if (cal.ok && std::isfinite(cal.handToWorldScale) && cal.handToWorldScale > 0.2f && cal.handToWorldScale < 5.0f)
        for (int i = 0; i < 3; ++i) pos[i] = f.headPos[i] + cal.handToWorldScale * (pos[i] - f.headPos[i]);
    f.ray = dvr::aim::from_pose(hand, true, pos, s.gripQuat, s.generation, s.stampMs, now);
    dvr::fireaim::Solution sol;
    if (!dvr::fireaim::solve(f, now, g_viewYawRad, g_viewPitchRad, camera, g_posScaleUU, camera, sol)) return false;
    memcpy(out, sol.origin, 12);
    return true;
#else
    (void)hand; (void)camera; (void)out;
    return false;
#endif
}

// interact_aim.cpp asks this before its own ray: while a target is held, the engine's
// interaction trace looks from the head at the target.
static bool PickupRay(float* origin, float* dir)
{
    if (!g_ppTarget || !PpGameplay()) return false;
    float camera[3];
    if (!GameCameraAnchor(camera)) return false;
    float d[3] = { g_ppTargetLoc[0] - camera[0], g_ppTargetLoc[1] - camera[1], g_ppTargetLoc[2] - camera[2] };
    if (!dvr::fireaim::normalize(d)) return false;
    memcpy(origin, camera, 12); memcpy(dir, d, 12);
    InterlockedIncrement(&g_ppRayDriven);
    return true;
}

static void PpDropTarget(const char* why, double nowMs, bool block)
{
    if (g_ppTarget) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 500,
            "pickup: target released (%s) after %.0f ms, engine focus on it %s", why, nowMs - g_ppTargetSinceMs,
            g_ppTargetFocused ? "YES" : "never");
        if (block) {
            g_ppBlockForMs = (g_ppBlock == g_ppTarget && g_ppBlockForMs > 0) ? (g_ppBlockForMs < 30000.0 ? g_ppBlockForMs * 2.0 : 60000.0) : 3000.0;
            g_ppBlock = g_ppTarget; g_ppBlockUntilMs = nowMs + g_ppBlockForMs;
        }
    }
    g_ppTarget = NULL; g_ppTargetHand = -1; g_ppTargetFocused = false;
    g_ppReadyMask.store(0);
}

// Script lane, once a frame.
static void PhysicalPickupTick()
{
    static uint32_t lastFrame = 0xffffffffu;
    const uint32_t frame = (uint32_t)dvr::frame::count();
    if (frame == lastFrame) return;
    lastFrame = frame;
    const double now = MaimNowMs();
    if (!PpGameplay()) { if (g_ppTarget) PpDropTarget("not in gameplay, or off", now, false); g_ppReadyMask.store(0); return; }

    PpSweep(2000, now);

    static uint32_t locOff = 0, focusOff = 0, hiddenOff = 0, hiddenMask = 0; static bool hiddenAsked = false;
    if (!locOff)   locOff   = RflOffsetOf("Actor", "Location");
    if (!hiddenAsked && locOff) {
        hiddenAsked = true;
        if (!FindBoolProp("Actor", "bHidden", &hiddenOff, &hiddenMask)) { hiddenOff = 0; hiddenMask = 0; }
        Log("pickup: Actor.Location at +0x%X, Actor.bHidden %s (a hidden pickup is never a target)", locOff,
            hiddenMask ? "resolved" : "NOT resolved - hidden items are left to the game's refusal");
    }
    if (!focusOff) focusOff = RflOffsetOf("DishonoredPlayerController", "m_pCrosshairActor");
    float camera[3], hand[2][3];
    bool handOk[2] = { false, false };
    if (!locOff || !focusOff || !GameCameraAnchor(camera)) { if (g_ppTarget) PpDropTarget("no camera or offsets", now, false); return; }
    for (int h = 0; h < 2; ++h) handOk[h] = PpHandWorld(h, camera, hand[h]);
    if (!handOk[0] && !handOk[1]) { if (g_ppTarget) PpDropTarget("no tracked hand", now, false); return; }

    void** objs = RangeReadable((void*)kGObjHdr, 12) ? *(void***)kGObjHdr : NULL;
    const uint32_t num = objs ? *(uint32_t*)(kGObjHdr + 4) : 0;
    const float reach = g_ppReachM.load() * g_posScaleUU;         // game units
    const float keep  = reach * 1.25f;                            // the held target's hysteresis
    ::dvr::mem::RegionMemo rt, ro;
    uint8_t* best = NULL; uint32_t bestIdx = 0; float bestD2 = 0, bestLoc[3] = {}; int bestHand = -1;
    float dist2[2] = { 1e30f, 1e30f };                            // of the chosen item, per hand
    if (objs && !((uintptr_t)objs & 3)) {
        for (const PpEntry& e : g_ppList) {
            if (e.idx >= num || !rt.ok(objs + e.idx, sizeof(void*)) || (uint8_t*)objs[e.idx] != e.obj) continue;
            if (!ro.ok(e.obj, locOff + 12) || *(uint8_t**)(e.obj + kClassOff) != e.cls) continue;
            if (e.obj == g_ppBlock && now < g_ppBlockUntilMs) continue;
            if (hiddenMask && ro.ok(e.obj + hiddenOff, 4) && (*(uint32_t*)(e.obj + hiddenOff) & hiddenMask)) continue;
            float loc[3]; memcpy(loc, e.obj + locOff, 12);
            if (!std::isfinite(loc[0]) || !std::isfinite(loc[1]) || !std::isfinite(loc[2])) continue;
            const float lim = (e.obj == g_ppTarget ? keep : reach); const float lim2 = lim * lim;
            float d2h[2] = { 1e30f, 1e30f }; int nearHand = -1;
            for (int h = 0; h < 2; ++h) {
                if (!handOk[h]) continue;
                const float dx = loc[0] - hand[h][0], dy = loc[1] - hand[h][1], dz = loc[2] - hand[h][2];
                d2h[h] = dx * dx + dy * dy + dz * dz;
                if (nearHand < 0 || d2h[h] < d2h[nearHand]) nearHand = h;
            }
            if (nearHand < 0 || d2h[nearHand] > lim2) continue;
            // the held target keeps the choice unless another item is clearly nearer
            const float score = d2h[nearHand] * (e.obj == g_ppTarget ? 0.6f : 1.0f);
            if (!best || score < bestD2) {
                best = e.obj; bestIdx = e.idx; bestD2 = score; bestHand = nearHand;
                memcpy(bestLoc, loc, 12); dist2[0] = d2h[0]; dist2[1] = d2h[1];
            }
        }
    }
    if (!best) { if (g_ppTarget) PpDropTarget("no lootable within reach", now, false); return; }

    if (best != g_ppTarget) {
        if (g_ppTarget) PpDropTarget("a nearer item", now, false);
        g_ppTarget = best; g_ppTargetIdx = bestIdx; g_ppTargetSinceMs = now; g_ppFocusedLastMs = 0; g_ppTargetFocused = false;
        const char* cn = ObjClassName(best);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 400,
            "pickup: target %s at %.0f uu (%.0f cm) from the %s hand, reach %.0f cm - the interaction trace now looks at it; "
            "the grip picks it up once the game focuses it",
            cn ? cn : "?", sqrtf(dist2[bestHand]), sqrtf(dist2[bestHand]) / g_posScaleUU * 100.0f, bestHand ? "RIGHT" : "LEFT",
            g_ppReachM.load() * 100.0f);
    }
    memcpy(g_ppTargetLoc, bestLoc, 12);
    g_ppTargetHand = bestHand;

    // The engine's verdict: is its focused actor our target?
    uint8_t* pc = g_peCtrl; uint8_t* focus = NULL;
    if (pc && LooksLikeObj(pc) && RangeReadable(pc + focusOff, 4)) focus = *(uint8_t**)(pc + focusOff);
    if (focus == g_ppTarget) { g_ppFocusedLastMs = now; g_ppTargetFocused = true; }
    else if (now - (g_ppFocusedLastMs > 0 ? g_ppFocusedLastMs : g_ppTargetSinceMs) > 250.0) {
        // Aimed at it for a quarter second and the game did not take it: not usable now.
        PpDropTarget("the game did not focus it (looted, hidden or blocked) - left alone for 3 s, doubling each time", now, true);
        return;
    }
    uint32_t mask = 0;
    if (focus == g_ppTarget)
        for (int h = 0; h < 2; ++h) if (handOk[h] && dist2[h] <= keep * keep) mask |= 1u << h;
    g_ppReadyMask.store(mask);
    g_ppReadyMs.store(GetTickCount64());

    static double nextBeat = 0;
    if (now >= nextBeat) {
        nextBeat = now + 30000;
        Log("pickup: beat - %u lootable actor(s) listed, sweep %u took %.0f ms of game time, trace driven %ld time(s), "
            "grips swallowed %u, Interact pressed %u, reach %.0f cm",
            (unsigned)g_ppList.size(), g_ppSweeps, g_ppLastSweepMs, (long)g_ppRayDriven, g_ppSwallowed.load(), g_ppFired.load(),
            g_ppReachM.load() * 100.0f);
    }
}

// Pad bridge (present thread). `raw` is the PHYSICAL snapshot about to be remapped: a grip that
// starts a pickup is zeroed in it until released, so whatever is bound to that grip does not
// fire. Returns true while Interact should be held down.
static bool PickupPadFilter(dvr::vr::InputSnapshot& raw, bool blocked)
{
    static bool was[2] = { false, false }, swallow[2] = { false, false };
    static uint64_t pressUntil = 0;
    const uint64_t now = GetTickCount64();
    const bool fresh = now - g_ppReadyMs.load() <= 250;
    const uint32_t mask = (g_ppOn.load() && !blocked && fresh) ? g_ppReadyMask.load() : 0;
    float* grip[2] = { &raw.gripL, &raw.gripR };
    for (int h = 0; h < 2; ++h) {
        const bool down = *grip[h] > (was[h] ? 0.7f : 0.9f);
        if (down && !was[h] && (mask & (1u << h))) {
            swallow[h] = true; pressUntil = now + 130;
            g_ppSwallowed.fetch_add(1); g_ppFired.fetch_add(1);
            Log("pickup: %s grip pressed with the target in reach and focused - grip swallowed, Interact pressed for 130 ms",
                h ? "RIGHT" : "LEFT");
        }
        if (!down) swallow[h] = false;
        was[h] = down;
        if (swallow[h]) *grip[h] = 0.0f;
    }
    return now < pressUntil;
}

static void PickupSet(bool on, const char* who)
{
    g_ppOn.store(on);
    if (on) InteractAimInstall();                       // the two bridges carry the target's ray
    if (!on) g_ppReadyMask.store(0);
    Log("pickup: physical pickup %s (%s) - reach %.0f cm. Reach a hand to loot and squeeze its grip; the Interact "
        "button is unchanged. Bridges first-pass=%s selector=%s",
        on ? "ON" : "off", who, g_ppReachM.load() * 100.0f, g_iaFirstDet.on ? "ready" : "NOT installed",
        g_iaSelDet.on ? "ready" : "NOT installed");
}

static void PickupSetReachCm(float cm)
{
    if (!std::isfinite(cm)) return;
    cm = cm < 10.0f ? 10.0f : cm > 80.0f ? 80.0f : cm;
    g_ppReachM.store(cm / 100.0f);
}
static float PickupReachCm() { return g_ppReachM.load() * 100.0f; }

static void PickupConfigure(const char* ini)
{
    PickupSetReachCm(IniFloat(ini, "Aim", "PhysicalPickupReachCm", 30));
    PickupSet(IniFloat(ini, "Aim", "PhysicalPickup", 1) != 0.0f, "ini [Aim] PhysicalPickup");
}

static bool PickupCommand(const char* args)
{
    bool b = false; float cm = 0;
    if (DvrOnOff(args, &b)) {
        PickupSet(b, "seam");
        ConfigWriteKey("Aim", "PhysicalPickup", b ? "1" : "0", "the seam");
        return true;
    }
    if (sscanf(args, "reach %f", &cm) == 1) {
        PickupSetReachCm(cm);
        char v[16]; _snprintf(v, sizeof(v), "%.0f", PickupReachCm()); v[sizeof(v) - 1] = 0;
        ConfigWriteKey("Aim", "PhysicalPickupReachCm", v, "the seam");
    }
    Log("pickup: on|off, reach <cm> (now %s, reach %.0f cm) | listed %u, sweeps %u (last %.0f ms), target %s (hand %d, focused %s), "
        "trace driven %ld, grips swallowed %u, Interact pressed %u, ready mask %u",
        g_ppOn.load() ? "ON" : "off", PickupReachCm(), (unsigned)g_ppList.size(), g_ppSweeps, g_ppLastSweepMs,
        g_ppTarget ? "held" : "none", g_ppTargetHand, g_ppTargetFocused ? "yes" : "no", (long)g_ppRayDriven,
        g_ppSwallowed.load(), g_ppFired.load(), g_ppReadyMask.load());
    return true;
}
