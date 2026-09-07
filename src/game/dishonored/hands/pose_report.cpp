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
    Log("pose: %d of %d name(s) resolved. A MISS has several possible causes "
        "and this line must not pick one: the property may be declared on a "
        "DIFFERENT owner class than the one asked (Mesh is on Engine.Pawn, not "
        "DishonoredPawn), it may be a FUNCTION rather than a property (the "
        "component's socket queries are), the owner class may not be loaded "
        "yet, or the build may genuinely differ from the corpus. The first two "
        "were the actual cause of both misses on the first run.",
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
static bool PrDumpSockets(void)
{
    if (!g_pePawn) { Log("pose/dump: no live pawn yet"); return false; }

    const uint32_t meshOff = PrOff("Pawn", "Mesh");
    if (!meshOff) {
        Log("pose/dump: Pawn.Mesh did not resolve, so the skeletal component "
            "cannot be reached from here. UNKNOWN, not assumed.");
        return false;
    }
    if (!RangeReadable(g_pePawn + meshOff, 4)) {
        Log("pose/dump: the pawn's Mesh slot at +0x%X is not readable", meshOff);
        return false;
    }
    uint8_t* mesh = *(uint8_t**)(g_pePawn + meshOff);
    if (!mesh || ((uintptr_t)mesh & 3) || !RangeReadable(mesh, 0x80)) {
        Log("pose/dump: the pawn's Mesh pointer is %p, which is not a readable "
            "object - reported rather than followed", (void*)mesh);
        return false;
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
        return false;
    }
    if (!RangeReadable(mesh + assetOff, 4)) {
        Log("pose/dump: the SkeletalMesh slot at +0x%X is unreadable", assetOff);
        return false;
    }
    uint8_t* asset = *(uint8_t**)(mesh + assetOff);
    if (!asset || ((uintptr_t)asset & 3) || !RangeReadable(asset, 0x200)) {
        Log("pose/dump: the SkeletalMesh pointer is %p, not a readable asset",
            (void*)asset);
        return false;
    }
    Log("pose/dump: component %p -> SkeletalMesh asset %p", (void*)mesh, (void*)asset);
    // A UE3 dynamic array is { void* data; int count; int max; }, so the count
    // is read before anything is walked and an absurd count refuses instead of
    // walking off into memory.
    if (!RangeReadable(asset + sockOff, 12)) {
        Log("pose/dump: the socket array header at +0x%X is unreadable", sockOff);
        return false;
    }
    uint8_t** data = *(uint8_t***)(asset + sockOff);
    const int n     = *(int*)(asset + sockOff + 4);
    const int cap = *(int*)(asset + sockOff + 8);
    if (n < 0 || n > 512 || cap < n || (n && (!data || ((uintptr_t)data & 3)))) {
        Log("pose/dump: the socket array reads count=%d capacity=%d data=%p, "
            "which is not a credible dynamic array - refusing to walk it",
            n, cap, (void*)data);
        return false;
    }
    Log("pose/dump: %d socket(s) declared (capacity %d)", n, cap);

    const uint32_t sn = PrOff("SkeletalMeshSocket", "SocketName");
    const uint32_t bn = PrOff("SkeletalMeshSocket", "BoneName");
    const uint32_t rl = PrOff("SkeletalMeshSocket", "RelativeLocation");
    if (!sn || !bn) {
        Log("pose/dump: SocketName/BoneName did not resolve, so the sockets "
            "cannot be named. UNKNOWN.");
        return false;
    }
    const uint32_t rr = PrOff("SkeletalMeshSocket", "RelativeRotation");
    const uint32_t rs = PrOff("SkeletalMeshSocket", "RelativeScale");
    // RelativeScale begins at +0x60 on this build, so a flat 0x60 guard would
    // not cover it. Guard to the end of the last field actually read.
    uint32_t need = 0x40;
    if (rl > need) need = rl; if (rr > need) need = rr; if (rs > need) need = rs;
    need += 12;
    int nRead = 0, nSkip = 0;
    for (int i = 0; i < n && i < 64; i++) {
        if (!RangeReadable((uint8_t*)&data[i], 4)) { nSkip++; break; }
        uint8_t* so = data[i];
        if (!so || ((uintptr_t)so & 3) || !RangeReadable(so, need)) { nSkip++; continue; }
        const char* socketName = NameFromIndex(*(uint32_t*)(so + sn));
        const char* boneName   = NameFromIndex(*(uint32_t*)(so + bn));
        // A field that cannot be read is UNKNOWN. The first build initialised
        // the location to zero and printed it either way, so an unreadable
        // field and a genuine origin looked identical - and a zero TRANSLATION
        // does not mean an identity frame in any case, which is why the
        // rotation and scale are printed beside it now.
        char loc[64] = "UNKNOWN", rot[64] = "UNKNOWN", scl[64] = "UNKNOWN";
        if (rl && RangeReadable(so + rl, 12)) {
            float v[3]; memcpy(v, so + rl, 12);
            _snprintf(loc, sizeof(loc), "%.2f %.2f %.2f", v[0], v[1], v[2]);
        }
        if (rr && RangeReadable(so + rr, 12)) {
            int32_t v[3]; memcpy(v, so + rr, 12);
            const float k = 180.0f / 32768.0f;
            _snprintf(rot, sizeof(rot), "%.2f %.2f %.2f deg",
                      v[0] * k, v[1] * k, v[2] * k);
        }
        if (rs && RangeReadable(so + rs, 12)) {
            float v[3]; memcpy(v, so + rs, 12);
            _snprintf(scl, sizeof(scl), "%.2f %.2f %.2f", v[0], v[1], v[2]);
        }
        Log("pose/dump:   socket '%s' -> owning bone '%s' | loc (%s) rot (%s) "
            "scale (%s)",
            socketName ? socketName : "?", boneName ? boneName : "?",
            loc, rot, scl);
        nRead++;
    }
    Log("pose/dump: %d socket(s) declared, %d read, %d skipped as unreadable%s. "
        "A socket names an ATTACHMENT frame and gives its OWNING bone - it does "
        "NOT give that bone's parent, so nothing here establishes whether an "
        "attachment joint is a child of the hand. It is also not a grip pose "
        "for a controller; that offset is a separate calibration this report "
        "does not measure.",
        n, nRead, nSkip,
        (n > 64) ? " and the walk was TRUNCATED at 64" : "");
    return true;
}


static void PrTick(void)
{
    if (!g_prOn) return;
    PrResolve();
    // Dump ONCE automatically as soon as a pawn is live. The tester cannot
    // reliably reach the command seam, so an instrument that has to be asked
    // for is an instrument that does not run.
    // RETRY UNTIL IT ACTUALLY SUCCEEDS. The first build set the done flag
    // before calling the dump, so a pawn whose mesh or asset was not ready yet
    // consumed the one automatic attempt and never tried again - a failure
    // that looked exactly like a completed measurement.
    if (!g_prDumped && g_prTried && g_pePawn) {
        const double now = MaimNowMs();
        if (now >= g_prDumpNext) {
            g_prDumpNext = now + 2000.0;
            g_prDumped = PrDumpSockets();
        }
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
