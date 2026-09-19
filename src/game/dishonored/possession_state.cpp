// VR-135: controlled possession, validated for the PRESENTATION verdict only.
// Included after stereo_state.cpp's dependencies (uobject, reflect, crouch).
//
// While the player possesses a rat, fish or person the controller's Pawn is a
// DisPossessablePawn subclass. The capsule liveness (PawnForCollision) refuses
// every non-player class on purpose, so the scene verdict read "no live pawn"
// and the re-entry fell to one draw for the whole possession (build452 log:
// stereo/state FALLBACK pawn=0 valid=0, beat mono/s=95..119).
//
// This does NOT relax that guard. It publishes a separate, read-only fact: the
// controller's current Pawn is a live possessable pawn whose own back-pointers
// name OUR live controller and a live player pawn. The engine sets both while
// the possession is active; neither is inferred from a class name alone. Only
// DvrSceneVerdict consumes it, and it keeps the menu, UI-surface, scene-upload
// and view terms. Gameplay permission (hands, aim, crouch) stays parked.
//
// LANE: the script lane (ProcessEvent), 50 ms cadence. Nothing is retained
// between samples except for change logging: every sample re-reads the chain
// from the controller and re-checks liveness against the current table.
#include <atomic>

static std::atomic<bool> g_possStereoOn{false};
static std::atomic<unsigned long long> g_possOkTick{0};

static bool PossessionStereoEnabled() { return g_possStereoOn.load(); }
static void PossessionStereoSet(bool on) {
    g_possStereoOn.store(on);
    Log("possession/stereo: enabled=%d (presentation only; the capsule and gameplay guards are unchanged)", on ? 1 : 0);
}
static void PossessionStereoConfigure(const char* ini) {
    PossessionStereoSet(GetPrivateProfileIntA("Cine", "PossessionStereo", 0, ini) != 0);
}

// Validated within the last 250 ms. The scene verdict still demands its own
// fresh camera uploads, a live view dispatch and no menu.
static bool PossessionStereoLive() {
    if (!PossessionStereoEnabled()) return false;
    const unsigned long long ok = g_possOkTick.load();
    const unsigned long long now = GetTickCount64();
    return ok && now >= ok && now - ok <= 250;
}

// Class name of a UClass object itself (its own FName), not of its class.
static const char* PossStructName(uint8_t* cls) {
    if (!cls || ((uintptr_t)cls & 3) || !RangeReadable(cls, kSuperFieldOff + 4)) return nullptr;
    return RealName(*(uint32_t*)(cls + kNameOff));
}

// Does obj's class descend from `base`, by the engine's own SuperField chain?
// -1 = the chain could not be read (caller falls back to the name list).
static int PossIsA(uint8_t* obj, const char* base) {
    if (!obj || !RangeReadable(obj + kClassOff, 4)) return -1;
    uint8_t* cls = *(uint8_t**)(obj + kClassOff);
    for (int depth = 0; cls && depth < 32; ++depth) {
        const char* n = PossStructName(cls);
        if (!n) return -1;
        if (!strcmp(n, base)) return 1;
        if (!strcmp(n, "Object")) return 0;
        cls = *(uint8_t**)(cls + kSuperFieldOff);
    }
    return -1;
}

// The ancestry walk is trusted only once it reproduces a published chain:
// the live player pawn must reach Pawn and Actor (ENGINE_NOTES, SuperField).
static int g_possChain = -1;   // -1 untested, 0 failed, 1 passed
static void PossVerifyChain(uint8_t* playerPawn) {
    int& verified = g_possChain;
    if (verified >= 0 || !playerPawn) return;
    const int pawn = PossIsA(playerPawn, "Pawn"), actor = PossIsA(playerPawn, "Actor");
    const int wrong = PossIsA(playerPawn, "DisPossessablePawn");
    verified = (pawn == 1 && actor == 1 && wrong == 0) ? 1 : 0;
    Log("possession/stereo: SuperField chain check on the player pawn: Pawn=%d Actor=%d DisPossessablePawn=%d (want 1/1/0) -> %s",
        pawn, actor, wrong, verified ? "ancestry walk TRUSTED" : "ancestry walk REFUSED, class name list only");
}

// Every DisPossessablePawn subclass in the shipped scripts (base game and both
// DLCs), the fallback when the ancestry walk is not trusted. Rats, fish and
// river krusts are not pawns: possessing one puts the player in a
// DisPossessionProxyPawn. The back-pointers are declared on DisPossessablePawn,
// so they are read only on a class that carries them.
static bool PossessableClass(uint8_t* pawn, const char* cls) {
    if (!cls) return false;
    if (g_possChain == 1) {
        const int a = PossIsA(pawn, "DisPossessablePawn");
        if (a >= 0) return a == 1;
    }
    static const char* const kClasses[] = {
        "DisPossessionProxyPawn", "DishonoredNPCPawn", "DisTallboyNPCPawn",
        "DisDLC06NPCPawn", "DisDLC06AssassinNPCPawn", "DisDLC06ButcherNPCPawn",
        "DisDLC06SummonedAssassinNPCPawn", "DisDLC07NPCPawn", "DisDLC07AssassinNPCPawn",
        "DisDLC07GravehoundNPCPawn", "DisDLC07SummonedAssassinNPCPawn", "DisDLC07TentacleNPCPawn" };
    for (const char* c : kClasses) if (!strcmp(cls, c)) return true;
    return false;
}

static uint8_t* PossReadPtr(uint8_t* o, uint32_t off) {
    if (!o || !off || !RangeReadable(o + off, sizeof(void*))) return nullptr;
    return *(uint8_t**)(o + off);
}

static void PossessionStateTick() {
    if (!PossessionStereoEnabled()) { g_possOkTick.store(0); return; }
    static unsigned long long nextSample = 0, nextRebuild = 0;
    const unsigned long long now = GetTickCount64();
    if (now < nextSample) return;
    nextSample = now + 50;

    static uint32_t pawnOff = 0, ctrlOff = 0, playerOff = 0, fxOff = 0, stageOff = 0;
    static bool resolved = false, stageOk = false;
    if (!resolved && RflNamesReady()) {
        pawnOff   = RflOffsetOf("Controller", "Pawn");
        ctrlOff   = RflOffsetOf("DisPossessablePawn", "m_pPossessingController");
        playerOff = RflOffsetOf("DisPossessablePawn", "m_pPossessingPlayerPawn");
        fxOff     = RflOffsetOf("DishonoredPlayerController", "m_PossessionEffectSettings");
        // A struct member at offset 0 is valid; RflOffsetOf reports 0 as a miss.
        stageOk   = FindPropOffsetChecked("DisPossessionEffectSettings", "m_Stage", &stageOff);
        resolved = true;
        Log("possession/stereo: layout Controller.Pawn=+0x%x DisPossessablePawn.m_pPossessingController=+0x%x "
            "m_pPossessingPlayerPawn=+0x%x effect=+0x%x stage=%s+0x%x%s",
            pawnOff, ctrlOff, playerOff, fxOff, stageOk ? "" : "MISSING ", stageOff,
            (pawnOff && ctrlOff && playerOff) ? "" : "  <-- REQUIRED FIELD MISSING, possession stays mono");
    }

    const char* why = "ok";
    uint8_t* ctrl = g_peCtrl;
    uint8_t* pawn = nullptr;
    uint8_t* player = nullptr;
    const char* cls = nullptr;
    int stage = -1;
    bool ok = false;
    if (!pawnOff || !ctrlOff || !playerOff) why = "layout unresolved";
    else if (!ctrl || !IsLiveObject(ctrl) || !LooksLikeObj(ctrl)) why = "controller not live";
    else if (!(pawn = PossReadPtr(ctrl, pawnOff))) why = "controller has no pawn";
    else if (!PossessableClass(pawn, cls = ObjClassName(pawn))) {
        why = "pawn is not possessable (ordinary play)";
        if (cls && !strcmp(cls, "DishonoredPlayerPawn") && IsLiveObject(pawn)) PossVerifyChain(pawn);
    }
    else if (!IsLiveObject(pawn)) {
        why = "possessable pawn not in the live table";
        // The proxy is spawned after the level's table was built. The capsule
        // tick rebuilds on its own failure when PawnFromController is on; this
        // covers the other configuration, bounded the same way.
        if (!g_pawnFromController && now >= nextRebuild) { nextRebuild = now + 1000; BuildLiveSet(); }
    }
    else if (PossReadPtr(pawn, ctrlOff) != ctrl) why = "pawn's possessing controller is not ours";
    else if (!(player = PossReadPtr(pawn, playerOff)) || !IsLiveObject(player)) why = "possessing player pawn not live";
    else {
        const char* pc = ObjClassName(player);
        if (!pc || !strstr(pc, "PlayerPawn")) why = "possessing player pawn has the wrong class";
        else ok = true;
    }
    if (fxOff && stageOk && ctrl && pawn) {
        unsigned char v = 255;
        if (RangeReadable(ctrl + fxOff + stageOff, 1)) { v = *(ctrl + fxOff + stageOff); stage = v <= 4 ? v : -1; }
    }
    g_possOkTick.store(ok ? now : 0);

    // Log the owner and the reason on a CHANGE only. A possessable pawn that
    // fails validation is the interesting line; ordinary play is silent after
    // the first sample.
    static const char* lastWhy = nullptr;
    static uint8_t* lastPawn = nullptr;
    static int lastStage = -2;
    if (why != lastWhy || pawn != lastPawn || stage != lastStage) {
        static const char* const kStage[] = { "Off", "Intro", "While", "Warning", "Outro" };
        Log("possession/stereo: %s ctrl=%p pawn=%p (%s) player=%p effectStage=%d(%s) reason=%s -> presentation %s",
            ok ? "VALIDATED" : "not validated", (void*)ctrl, (void*)pawn, cls ? cls : "-", (void*)player,
            stage, stage >= 0 ? kStage[stage] : "unavailable", why,
            ok ? "may be stereo (menu/scene/view terms still apply)" : "unchanged by possession");
        lastWhy = why; lastPawn = pawn; lastStage = stage;
    }
}
