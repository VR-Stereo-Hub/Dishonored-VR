// game/dishonored/hands/pose_report.cpp - VR-33 step 1. READ-ONLY.
//
// Resolves the engine's own names for the things the placement work needs -
// sockets, their parent bones, the equipped item, the hand controls - and says
// which ones this build could find. See state chunk 56 for why the previous
// four attempts made this necessary.
//
// Nothing here writes engine memory. Nothing here infers a value from a
// missing field.

// Resolve every name in the table once. Retried rather than failed
// permanently, because GNames is empty until the exe's static initializers
// have run - the same rule config.cpp and arm_follow.cpp already follow.
static void PrResolve(void)
{
    if (g_prTried || !g_prOn) return;
    const uint32_t now = (uint32_t)GetTickCount64();
    if (now < g_prNextTryMs) return;
    g_prNextTryMs = now + 3000;

    if (!RangeReadable((void*)kGNamesData, 8)) return;
    const char* n0 = NameFromIndex(0);
    if (!n0 || strcmp(n0, "None") != 0) return;      // GNames sane?

    g_prTried = true;
    g_prFound = g_prMissing = 0;
    for (int i = 0; i < PR_MAX_FIELDS && g_prFields[i].cls; i++) {
        g_prFields[i].off = FindPropOffset(g_prFields[i].cls, g_prFields[i].prop);
        if (g_prFields[i].off) g_prFound++; else g_prMissing++;
    }

    Log("pose: ==== VR-33 step 1, the read-only correspondence report ====");
    Log("pose: %d of %d name(s) resolved. Every one below is quoted from the "
        "decompiled scripts, so a MISS means the running build's layout "
        "differs from the corpus - which is a finding, not a failure, and "
        "nothing is inferred from it.",
        g_prFound, g_prFound + g_prMissing);
    for (int i = 0; i < PR_MAX_FIELDS && g_prFields[i].cls; i++) {
        if (g_prFields[i].off)
            Log("pose:   OK      %s.%s = +0x%X",
                g_prFields[i].cls, g_prFields[i].prop, g_prFields[i].off);
        else
            Log("pose:   UNKNOWN %s.%s - not found by reflection on this build",
                g_prFields[i].cls, g_prFields[i].prop);
    }
    Log("pose: what this does NOT yet establish, and must not be assumed: "
        "which socket each hand uses, whether a palette entry's translation is "
        "a joint or a bind origin, the viewmodel's projection scale, and the "
        "order the native attachment update runs in. Those are steps 1b to 3 "
        "of docs/dishonored/VR-33-REVISED-PLAN.md.");
}


// Look up one resolved offset, or 0.
static uint32_t PrOff(const char* cls, const char* prop)
{
    for (int i = 0; i < PR_MAX_FIELDS && g_prFields[i].cls; i++)
        if (!strcmp(g_prFields[i].cls, cls) && !strcmp(g_prFields[i].prop, prop))
            return g_prFields[i].off;
    return 0;
}


// Walk the live pawn to its skeletal component and report the sockets the
// asset actually declares, with each socket's parent BONE and local offset.
//
// This is the thing that replaces guessing the wrist from a vertex cloud: a
// socket names its bone, and the engine states both. It is read entirely
// through RangeReadable guards, and any pointer that does not survive one is
// reported rather than followed.
static void PrDumpSockets(void)
{
    if (!g_pePawn) { Log("pose/dump: no live pawn yet"); return; }

    const uint32_t meshOff = PrOff("Pawn", "Mesh");
    if (!meshOff) {
        Log("pose/dump: Pawn.Mesh did not resolve, so the skeletal component "
            "cannot be reached from here. UNKNOWN, not assumed.");
        return;
    }
    if (!RangeReadable(g_pePawn + meshOff, 4)) {
        Log("pose/dump: the pawn's Mesh slot at +0x%X is not readable", meshOff);
        return;
    }
    uint8_t* mesh = *(uint8_t**)(g_pePawn + meshOff);
    if (!mesh || ((uintptr_t)mesh & 3) || !RangeReadable(mesh, 0x80)) {
        Log("pose/dump: the pawn's Mesh pointer is %p, which is not a readable "
            "object - reported rather than followed", (void*)mesh);
        return;
    }
    Log("pose/dump: pawn %p -> SkeletalMeshComponent %p", (void*)g_pePawn, (void*)mesh);

    // SOCKETS LIVE ON THE ASSET, not on the component: component ->
    // SkeletalMesh -> Sockets. The component declares socket QUERIES, which are
    // functions and which property reflection will never find.
    const uint32_t assetOff = PrOff("SkeletalMeshComponent", "SkeletalMesh");
    const uint32_t sockOff  = PrOff("SkeletalMesh", "Sockets");
    if (!assetOff || !sockOff) {
        Log("pose/dump: SkeletalMeshComponent.SkeletalMesh (+0x%X) or "
            "SkeletalMesh.Sockets (+0x%X) did not resolve - the socket list is "
            "UNKNOWN on this build", assetOff, sockOff);
        return;
    }
    if (!RangeReadable(mesh + assetOff, 4)) {
        Log("pose/dump: the SkeletalMesh slot at +0x%X is unreadable", assetOff);
        return;
    }
    uint8_t* asset = *(uint8_t**)(mesh + assetOff);
    if (!asset || ((uintptr_t)asset & 3) || !RangeReadable(asset, 0x200)) {
        Log("pose/dump: the SkeletalMesh pointer is %p, not a readable asset",
            (void*)asset);
        return;
    }
    Log("pose/dump: component %p -> SkeletalMesh asset %p", (void*)mesh, (void*)asset);
    // A UE3 dynamic array is { void* data; int count; int max; }, so the count
    // is read before anything is walked and an absurd count refuses instead of
    // walking off into memory.
    if (!RangeReadable(asset + sockOff, 12)) {
        Log("pose/dump: the socket array header at +0x%X is unreadable", sockOff);
        return;
    }
    uint8_t** data = *(uint8_t***)(asset + sockOff);
    const int n     = *(int*)(asset + sockOff + 4);
    if (n < 0 || n > 512 || (n && (!data || ((uintptr_t)data & 3)))) {
        Log("pose/dump: the socket array reads count=%d data=%p, which is not "
            "credible - refusing to walk it", n, (void*)data);
        return;
    }
    Log("pose/dump: %d socket(s) declared on this component", n);

    const uint32_t sn = PrOff("SkeletalMeshSocket", "SocketName");
    const uint32_t bn = PrOff("SkeletalMeshSocket", "BoneName");
    const uint32_t rl = PrOff("SkeletalMeshSocket", "RelativeLocation");
    if (!sn || !bn) {
        Log("pose/dump: SocketName/BoneName did not resolve, so the sockets "
            "cannot be named. UNKNOWN.");
        return;
    }
    for (int i = 0; i < n && i < 64; i++) {
        if (!RangeReadable((uint8_t*)&data[i], 4)) break;
        uint8_t* so = data[i];
        if (!so || ((uintptr_t)so & 3) || !RangeReadable(so, 0x60)) continue;
        const char* socketName = NameFromIndex(*(uint32_t*)(so + sn));
        const char* boneName   = NameFromIndex(*(uint32_t*)(so + bn));
        float loc[3] = { 0.0f, 0.0f, 0.0f };
        if (rl && RangeReadable(so + rl, 12)) memcpy(loc, so + rl, 12);
        Log("pose/dump:   socket '%s' -> bone '%s'  local (%.2f, %.2f, %.2f)",
            socketName ? socketName : "?", boneName ? boneName : "?",
            loc[0], loc[1], loc[2]);
    }
    Log("pose/dump: a socket names an ATTACHMENT frame. It is not yet a grip "
        "pose for a controller, and the offset between the two is a separate "
        "calibration that no part of this report measures.");
}


static void PrTick(void)
{
    if (!g_prOn) return;
    PrResolve();
    // Dump ONCE automatically as soon as a pawn is live. The tester cannot
    // reliably reach the command seam, so an instrument that has to be asked
    // for is an instrument that does not run.
    if (!g_prDumped && g_prTried && g_pePawn) {
        g_prDumped = true;
        PrDumpSockets();
    }
    if (g_prDumpReq) { g_prDumpReq = 0; PrDumpSockets(); }
    if (g_prTried && g_prMissing) {
        const double now = MaimNowMs();
        if (now >= g_prNextReport) {
            g_prNextReport = now + 60000.0;
            Log("pose: %d name(s) still UNKNOWN on this build - see the list "
                "above. Nothing downstream may assume a value for them.",
                g_prMissing);
        }
    }
}


static bool PrCommand(const char* args)
{
    if (args) {
        while (*args == ' ') args++;
        if (!strncmp(args, "dump", 4)) { PrDumpSockets(); return true; }
        if (!strncmp(args, "again", 5)) { g_prTried = false; g_prNextTryMs = 0; }
    }
    Log("pose: status - %s, %d resolved, %d unknown. `pose dump` walks the "
        "live pawn's sockets, `pose again` re-resolves. READ-ONLY: this module "
        "never writes engine memory.",
        g_prOn ? "on" : "off", g_prFound, g_prMissing);
    return true;
}
