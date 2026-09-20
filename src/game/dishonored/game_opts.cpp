// game/dishonored/game_opts.cpp - VR-157: READ the game's own option profile.
// Included by src/mod/dishonoredvr.cpp (unity build) after ue3/uobject.cpp,
// whose FindFunctionObj / ObjClassName / IsLiveObject this needs.
//
// READ ONLY. Nothing here writes engine memory, the game's inis, or the
// profile blob. It exists to answer, once, three questions a session kept
// guessing at:
//
//   1. Where do the twelve player-facing option settings actually live?
//      Not in the 21 game inis: in the Steam Cloud profile blob
//      (userdata/<id>/205100/remote/OPTIONS.sav, a bit-packed UE3
//      OnlineProfileSettings). The inis carry a MIRROR of some of them,
//      and the mirror has already been caught disagreeing with the menu.
//
//   2. What has the RUNNING game resolved each one to? That is the only
//      value that matters, and it is not what either ini says.
//
//   3. Which of the two names in the gamepad profile - bAutoAim and
//      bFriction - is the menu's "Auto Aim" and which is its "Aim Assist"?
//
// HOW IT READS, AND WHY THAT WAY. It never walks the ProfileSettings array
// by hand. The struct chain (OnlineProfileSetting -> SettingsProperty ->
// SettingsData) has three enum fields whose packed widths this project has
// not measured, and a guessed stride would produce a table of confident
// nonsense. Instead it asks the engine through its own accessors, over the
// ProcessEvent route console.cpp already proves:
//
//   GetProfileSettings()          on the player controller -> the object
//   GetProfileSettingName(id)     -> the id's own FName, so the log names
//                                    what it read instead of trusting a table
//   GetProfileSettingValueInt(id) -> the value
//
// The engine walks its own array; no layout constant is invented. The one
// thing that IS asserted - the parms block for each call - is checked by its
// result: GetProfileSettingName must hand back a name that matches the PSI
// spelling we asked for, and the run refuses and says so when it does not.
//
// The graphics half is read from the other side as well. `scale get <key>`
// goes to FSystemSettings::Exec, which reports what the RENDERER holds, not
// what an ini file says. Where a setting has both a profile entry and a
// SystemSettings key, the log prints both on one line, so the answer to
// "which one wins" is arithmetic rather than opinion.

namespace {

// The twelve the player asked for. `sysKey` is the [SystemSettings] mirror
// where one exists, NULL where the setting has no renderer-side twin.
//
// The ids come from tools/uscript/dishonored/Engine/OnlineProfileSettings.uc
// (enum EProfileSettingID). `psiName` is that enum's spelling with the PSI_
// prefix dropped, which is what UE3 registers the setting's FName as; the
// probe verifies it rather than assuming it.
struct GoEntry {
    int         id;
    const char* psiName;     // expected FName, VERIFIED against the engine
    const char* menuLabel;   // what the player sees in the options menu
    const char* want;        // the target this project wants a fresh install at
    const char* sysKey;      // [SystemSettings] mirror, or NULL
};

const GoEntry kGoTable[] = {
    // --- Gameplay -------------------------------------------------------
    { 105, "Gameplay_KillCamMode",            "Kill Cam",             "Off (0)",      NULL },
    { 108, "Gameplay_HeadBobAmount",          "Head Bob Amount",      "0",            NULL },
    { 109, "Gameplay_CameraRelativeClimbing", "Chain Climbing",       "not relative (0)", NULL },
    // --- User interface -------------------------------------------------
    {  99, "HUD_CrosshairStyle",              "Crosshair Style",      "Off (0)",      NULL },
    // --- Gamepad: which of these two is "Aim Assist" is THE open question
    {  81, "Gamepad_bAutoAim",                "Auto Aim (expected)",  "0",            NULL },
    {  83, "Gamepad_bFriction",               "Aim Assist (expected)","0",            NULL },
    {  82, "Gamepad_AutoAimStrength",         "(strength of 81)",     "-",            NULL },
    {  84, "Gamepad_FrictionStrength",        "(strength of 83)",     "-",            NULL },
    // --- Graphics -------------------------------------------------------
    { 116, "GraphicsPC_bFullScreen",          "Fullscreen",           "1",            "Fullscreen" },
    { 117, "GraphicsPC_bVSync",               "Vsync",                "0",            "UseVsync" },
    { 120, "GraphicsPC_ModelDetails",         "Model Details",        "High (1)",     "DetailMode" },
    { 121, "GraphicsPC_LightShaftEnable",     "Light Shafts",         "0",            "bAllowLightShafts" },
    { 122, "GraphicsPC_AntiAliasingMode",     "Antialiasing",         "MLAA (1)",     "iType_AntiAlias" },
    { 123, "GraphicsPC_RatShadows",           "Rat Shadows",          "0",            "bAllowRatsShadow" },
};
const int kGoCount = (int)(sizeof(kGoTable) / sizeof(kGoTable[0]));

// Grace between "gameplay is live" and the automatic read. Long enough that a
// level load's script burst is over, short enough that it lands inside any
// playtest worth reading.
const int kGoAutoDelayMs = 4000;

// A request, drained on the script lane next to DvrConsoleApply.
// 0 = idle, 1 = profile + SystemSettings, 2 = SystemSettings only (no
// player controller needed beyond the console's own).
volatile long g_goReq = 0;

// VR-157: IT MUST ASK ITSELF. The tester plays in a headset and cannot alt-tab
// to a prompt, so a diagnostic that only fires from `game-cmd.ps1` is a
// diagnostic that never fires - which is exactly what happened on its first
// build: the seam word shipped, two runs went by, and the log had no
// `gameopts` line in it at all. The repo's own PR template already says a seam
// word with no F10 control is not shipped; this is that rule with the extra
// step that the answer needs no interaction whatever.
//
// So: it runs ONCE per session, on its own, a few seconds after gameplay is
// first verified (the profile object hangs off the player controller, which
// does not exist on the title screen). Read-only, one burst of lines, then
// silent. `[Diagnostics] GameOptsOnStart=0` opts out; the F10 button and the seam
// word re-run it on demand.
bool   g_goAuto     = true;    // [Diagnostics] GameOptsOnStart
bool   g_goAutoDone = false;   // fired for this session
double g_goReadyMs  = 0.0;     // when gameplay was first seen

// Latched UFunctions. Looked up once; a miss is logged with the name so a
// rename in a future game build reads as a miss and not as a zero.
uint8_t* g_goFnGetSettings = NULL;   // DishonoredPlayerController.GetProfileSettings
uint8_t* g_goFnName        = NULL;   // OnlinePlayerStorage.GetProfileSettingName
uint8_t* g_goFnValueInt    = NULL;   // OnlinePlayerStorage.GetProfileSettingValueInt
bool     g_goFnsTried      = false;

// UE3 FName as it appears in a parms block: name-table index plus instance
// number. Number 0 means the bare name.
struct GoFName { int32_t index; int32_t number; };

void GoLatchFns()
{
    if (g_goFnsTried) return;
    g_goFnsTried = true;
    g_goFnGetSettings = FindFunctionObj("GetProfileSettings");
    g_goFnName        = FindFunctionObj("GetProfileSettingName");
    g_goFnValueInt    = FindFunctionObj("GetProfileSettingValueInt");
    Log("gameopts: UFunctions GetProfileSettings=%p GetProfileSettingName=%p "
        "GetProfileSettingValueInt=%p (a NULL here is a miss, not a zero)",
        (void*)g_goFnGetSettings, (void*)g_goFnName, (void*)g_goFnValueInt);
}

// VR-161: find the profile object by WALKING GObjects, which is what the first
// run forced.
//
// MEASURED 2026-09-20: `GetProfileSettings` resolved fine as a UFunction
// (0x16842F00) and, called through ProcessEvent on the latched player
// controller, returned 00000000. Every one of the fourteen rows came back
// empty. Either the parms block for that native is not a bare return pointer,
// or the object does not hang off the controller the way the script signature
// suggests.
//
// Rather than keep guessing at a parms block, ask the object table. The class
// name is the thing we actually know, `FindFunctionObj` already proves this
// walk, and the result is checked the same way: it must be live and its class
// must name a ProfileSettings. This does not care which class owns the
// accessor or what its calling convention is.
uint8_t* GoScanForProfileObject()
{
    if (!RangeReadable((void*)kGObjHdr, 12)) return NULL;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) {
        Log("gameopts: REFUSED - object table looks wrong (objs=%p num=%u)", (void*)objs, onum);
        return NULL;
    }
    // MEASURED 2026-09-20 (run 2): the scan found FIVE live *ProfileSettings and
    // this loop took the first one, which was `OnlineProfileSettings` - the base
    // class. All fourteen ids then refused. The game's own settings live on the
    // `ArkProfileSettings` subclass (tools/uscript/.../Engine/ArkProfileSettings.uc),
    // so "first match" was the wrong rule and a base-class instance answered
    // nothing, exactly as it should.
    //
    // Rank instead of taking the first, and PRINT EVERY CANDIDATE. The choice
    // being invisible is what made the first run's failure ambiguous: a refusal
    // on the wrong object reads identically to a refusal on the right one.
    struct Cand { uint8_t* obj; const char* cls; int score; int32_t entries; };
    Cand cands[16]; int nc = 0; int hits = 0;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || !strstr(cn, "ProfileSettings")) continue;
        // A class-name match catches the CLASS objects and the defaults too;
        // only a live instance can answer a value.
        if (!IsLiveObject(o)) continue;
        if (!strncmp(cn, "Default__", 9)) continue;
        ++hits;
        // Most derived wins: Ark is the game's own, then anything that is not
        // the bare base class, then the base class as a last resort.
        const int score = strstr(cn, "Ark") ? 3 : (strcmp(cn, "OnlineProfileSettings") ? 2 : 1);
        if (nc < 16) { cands[nc].obj = o; cands[nc].cls = cn; cands[nc].score = score; cands[nc].entries = -1; ++nc; }
    }
    if (!nc) {
        Log("gameopts: REFUSED - no live *ProfileSettings object in %u objects. The settings "
            "may not be loaded until the options menu has been opened once this session.", onum);
        return NULL;
    }
    // RANK BY DATA, NOT BY CLASS NAME. Run 4 measured that the top-ranked
    // ArkProfileSettings had a ProfileSettings array of ZERO entries, so every
    // id refused - correctly, because there was nothing to answer about. Class
    // name says which object is most derived; it does not say which one is
    // LOADED, and loaded is the only property that matters here. There were two
    // ArkProfileSettings and two UIDataProvider_OnlineProfileSettings in the
    // table, and this only ever looked at one of them.
    //
    // So: read every candidate's array length first and prefer a populated one,
    // breaking ties by how derived it is. Print the length beside each, because
    // "empty" and "wrong object" produced identical refusals for three runs.
    const uint32_t offArr = RflOffsetOf("OnlinePlayerStorage", "ProfileSettings");
    int best = -1;
    Log("gameopts: object-table scan found %d live *ProfileSettings (%d listed). Ranking by "
        "whether the settings array is actually POPULATED, then by how derived the class is:",
        hits, nc);
    for (int i = 0; i < nc; ++i) {
        uint8_t* d = NULL; int32_t n = -1;
        if (offArr) RflArrayAt(cands[i].obj, offArr, &d, &n);
        cands[i].entries = n;
        const bool loaded = n > 0;
        if (best < 0) best = i;
        else {
            const bool bl = cands[best].entries > 0;
            if ((loaded && !bl) || (loaded == bl && cands[i].score > cands[best].score)) best = i;
        }
    }
    for (int i = 0; i < nc; ++i)
        Log("gameopts:   %s %p %-40s score=%d entries=%d%s",
            i == best ? "USING  " : "       ", (void*)cands[i].obj, cands[i].cls,
            cands[i].score, cands[i].entries,
            cands[i].entries == 0 ? "  (EMPTY: cannot answer anything)" : "");
    if (best >= 0 && cands[best].entries <= 0)
        Log("gameopts: WARNING - EVERY candidate's settings array is empty, so the profile is "
            "not loaded anywhere in the object table. The refusals below are expected and are "
            "NOT a parms-block fault. The settings most likely arrive only once the options "
            "menu has been opened, or the async profile read has not completed this session.");
    if (best < 0) return NULL;
    return cands[best].obj;
}

// The ArkProfileSettings object. Asks the controller's own native first,
// because that is the route the script declares, and falls back to the object
// table when it hands back nothing - which is what happened on the first run.
// Returns NULL and says why on every failure path.
uint8_t* GoProfileObject()
{
    if (!g_peCtrl) {
        Log("gameopts: REFUSED - no PlayerController latched yet; run this in "
            "GAMEPLAY, not on the title screen");
        return NULL;
    }
    if (!g_goFnGetSettings) {
        Log("gameopts: REFUSED - GetProfileSettings is not in the name tables");
        return NULL;
    }
    struct { void* ReturnValue; } parms;
    memset(&parms, 0, sizeof(parms));
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(g_peCtrl, g_goFnGetSettings, &parms, NULL);
    g_peReentry = false;
    uint8_t* obj = (uint8_t*)parms.ReturnValue;
    if (!obj || ((uintptr_t)obj & 3) || !RangeReadable(obj, kClassOff + 4)) {
        Log("gameopts: GetProfileSettings returned %p (null, misaligned or unreadable) "
            "- falling back to the object table", (void*)obj);
        return GoScanForProfileObject();
    }
    const char* cn = ObjClassName(obj);
    if (!cn || !strstr(cn, "ProfileSettings")) {
        Log("gameopts: REFUSED - GetProfileSettings returned a '%s', not a "
            "ProfileSettings; the parms block for this native is wrong",
            cn ? cn : "(no class name)");
        return NULL;
    }
    if (!IsLiveObject(obj)) {
        Log("gameopts: REFUSED - the %s at %p is not in the live set",
            cn, (void*)obj);
        return NULL;
    }
    Log("gameopts: profile object %p is a live %s", (void*)obj, cn);
    return obj;
}

// The id's own FName, as the engine spells it. This is the instrument's
// self-check: an id whose name does not match the PSI table means the table
// is wrong for this build, and the value on that line must not be believed.
const char* GoSettingName(uint8_t* obj, int id)
{
    if (!g_goFnName) return NULL;
    struct { int32_t Id; GoFName ReturnValue; } parms;
    memset(&parms, 0, sizeof(parms));
    parms.Id = id;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(obj, g_goFnName, &parms, NULL);
    g_peReentry = false;
    if (parms.ReturnValue.index <= 0) return NULL;
    return NameFromIndex((uint32_t)parms.ReturnValue.index);
}

// The value. `ok` distinguishes "the engine says 0" from "the engine refused
// to answer" - without it a missing id reads as a setting that is turned off,
// which is exactly the zero-by-design trap this project has already paid for.
bool GoSettingValue(uint8_t* obj, int id, int32_t* out)
{
    *out = 0;
    if (!g_goFnValueInt) return false;
    struct { int32_t Id; int32_t Value; int32_t ReturnValue; } parms;
    memset(&parms, 0, sizeof(parms));
    parms.Id = id;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(obj, g_goFnValueInt, &parms, NULL);
    g_peReentry = false;
    *out = parms.Value;
    return parms.ReturnValue != 0;
}

// What the RENDERER holds, straight from FSystemSettings::Exec. This is the
// side of the answer no ini file can give: DishonoredEngine.ini has been
// observed carrying bAllowLightShafts=True while the menu reported the
// setting off, so the ini is a mirror and this is the object.
void GoReadSystemSetting(const char* key, char* out, int cap)
{
    out[0] = 0;
    if (!key) return;
    char cmd[128];
    _snprintf(cmd, sizeof(cmd), "scale get %s", key);
    cmd[sizeof(cmd) - 1] = 0;
    wchar_t w[128];
    MultiByteToWideChar(CP_UTF8, 0, cmd, -1, w, 128);
    char reply[256] = "";
    const int n = RunConsole(w, reply, sizeof(reply));
    if (n <= 0) {
        _snprintf(out, cap, "(scale get returned %d)", n);
        out[cap - 1] = 0;
        return;
    }
    _snprintf(out, cap, "%s", reply);
    out[cap - 1] = 0;
}

} // namespace

// Queued by the seam, run here on the script lane so the engine sees a normal
// game-thread caller - the same contract DvrConsoleApply works under, and for
// the same reason (RunConsole re-enters our own ProcessEvent hook).
static void GameOptsApply()
{
    // The self-ask. Waits for a player controller AND a settled gameplay
    // verdict, then a short grace so the profile read does not land in the
    // middle of a level load's script burst.
    if (g_goAuto && !g_goAutoDone && !g_goReq && g_peCtrl && DvrGameplayVerdict()) {
        const double now = MaimNowMs();
        if (g_goReadyMs <= 0.0) {
            g_goReadyMs = now;
            Log("gameopts: gameplay reached; the automatic read fires in %d ms "
                "([Diagnostics] GameOptsOnStart=0 turns it off)", kGoAutoDelayMs);
        } else if (now - g_goReadyMs >= (double)kGoAutoDelayMs) {
            g_goAutoDone = true;
            InterlockedExchange(&g_goReq, 1);
            Log("gameopts: automatic read (nobody asked for it; this is the one "
                "per session, so a headset run produces the answer by itself)");
        }
    }
    const long req = InterlockedExchange(&g_goReq, 0);
    if (!req) return;
    if (g_peReentry) { InterlockedExchange(&g_goReq, req); return; }

    GoLatchFns();
    uint8_t* obj = (req == 1) ? GoProfileObject() : NULL;

    // VR-161, run 3: the object is now demonstrably the right one
    // (ArkProfileSettings, ranked and printed) and ALL FOURTEEN ids still
    // refused. So "wrong object" is eliminated and two hypotheses remain, which
    // look identical from the accessor's return value alone:
    //
    //   (a) the profile's own ProfileSettings array is EMPTY - the settings are
    //       not loaded yet, and no accessor can answer about an id that is not
    //       in the array. That would also explain GetProfileSettings returning
    //       null: the subsystem is simply not up.
    //   (b) the array is populated and OUR parms blocks are wrong.
    //
    // Reading the array's LENGTH separates them, and needs no native call at
    // all - it is the same name-keyed resolver plus the TArray reader the rest
    // of the mod uses. A number here is worth more than another guess at a
    // calling convention.
    if (obj) {
        const uint32_t off = RflOffsetOf("OnlinePlayerStorage", "ProfileSettings");
        uint8_t* pdata = NULL; int32_t pnum = -1;
        if (!off) {
            Log("gameopts: OnlinePlayerStorage.ProfileSettings did not resolve by name, so the "
                "array length cannot be read; the refusals below are UNEXPLAINED");
        } else if (!RflArrayAt(obj, off, &pdata, &pnum)) {
            Log("gameopts: ProfileSettings array at +0x%x is unreadable on %p - the refusals "
                "below are UNEXPLAINED", off, (void*)obj);
        } else {
            Log("gameopts: ProfileSettings array at +0x%x holds %d entr%s. %s",
                off, (int)pnum, pnum == 1 ? "y" : "ies",
                pnum <= 0
                  ? "EMPTY: the profile is not loaded, so NO accessor can answer any id and the "
                    "refusals below are expected, not a parms-block fault. The settings probably "
                    "arrive only once the options menu has been opened, or the async profile "
                    "read has not completed."
                  : "POPULATED: the settings ARE here, so a refusal below is OUR call being "
                    "wrong, not the profile being absent. That is the thing to fix next.");
        }
    }
    // VR-161, run 5: THE ACCESSORS ARE THE PROBLEM, AND NOTHING ELSE IS.
    //
    // The eliminations are now complete. The object is right (ArkProfileSettings,
    // ranked and printed). The array is POPULATED - 115 entries. And all
    // fourteen ids still refuse. So it is not the wrong object and not an
    // unloaded profile: `GetProfileSettingName` and `GetProfileSettingValueInt`
    // called through ProcessEvent do not work here, for a reason the return
    // value cannot express.
    //
    // The original header said this file would never walk the array by hand,
    // because the OnlineProfileSetting -> SettingsProperty -> SettingsData chain
    // has enum fields of unmeasured width and a guessed stride would produce
    // confident nonsense. That reasoning was right, and the answer is not to
    // guess the stride now - it is to MEASURE it. With 115 entries in hand the
    // data can name its own layout: PropertyId is the first field of
    // SettingsProperty, the ids are a known set (the PSI enum runs to 153), and
    // a stride that is correct makes recognisable ids appear at a fixed step
    // while a wrong one produces noise.
    //
    // So this dumps the head of the array as raw dwords, once, and says what it
    // is for. Deriving the stride from that is offline work on real bytes,
    // which is the thing this project's rules actually ask for.
    if (obj) {
        const uint32_t off = RflOffsetOf("OnlinePlayerStorage", "ProfileSettings");
        uint8_t* pdata = NULL; int32_t pnum = 0;
        if (off && RflArrayAt(obj, off, &pdata, &pnum) && pdata && pnum > 0) {
            // The first version batched with `(i & 11) == 11`, a bitmask where a
            // modulo was meant, so the printed index ranges overlapped and the
            // stream could not be reassembled. Six dwords a line, indices exact.
            const int dwords = 96;   // 16 rows: enough to see a stride repeat
            if (RangeReadable(pdata, dwords * 4)) {
                const uint32_t* d = (const uint32_t*)pdata;
                for (int i = 0; i < dwords; i += 6)
                    Log("gameopts/raw:   [%2d] %08x %08x %08x %08x %08x %08x",
                        i, d[i], d[i+1], d[i+2], d[i+3], d[i+4], d[i+5]);
                // The shape is already visible in the first dump: small values
                // in the PSI range (12, 13, 16, 29, 30, 31, 37, 55, 59...) recur
                // beside 1s and 2s that look like the Owner and Type enums. If
                // OnlineProfileSetting is { Owner, PropertyId, Type, Value1,
                // Value2, AdvertisementType } then the stride is 6 dwords, and
                // this prints one candidate entry per line so that is checkable
                // by eye rather than asserted.
                Log("gameopts/raw: %d entries at %p, printed 6 dwords per line. If the stride IS "
                    "6 dwords then each line is one setting and column 2 is its PropertyId - the "
                    "PSI enum runs to 153, so a column of small plausible ids confirms it and a "
                    "column of noise refutes it. Do not assume; read the column.",
                    (int)pnum, (void*)pdata);
            } else {
                Log("gameopts/raw: array data at %p is not readable for %d dwords",
                    (void*)pdata, dwords);
            }
        }
    }
    Log("gameopts: ---- the game's own option settings, READ ONLY ----");
    Log("gameopts: profile = the Steam Cloud blob (OPTIONS.sav); system = what "
        "FSystemSettings holds now. A disagreement is the finding, not an error.");

    int named = 0, mismatched = 0, answered = 0, refused = 0;
    for (int i = 0; i < kGoCount; ++i) {
        const GoEntry& e = kGoTable[i];
        const char* engineName = obj ? GoSettingName(obj, e.id) : NULL;
        int32_t v = 0;
        const bool ok = obj ? GoSettingValue(obj, e.id, &v) : false;
        char sys[160] = "";
        if (e.sysKey) GoReadSystemSetting(e.sysKey, sys, sizeof(sys));

        const bool nameOk = engineName && !strcmp(engineName, e.psiName);
        if (engineName) ++named;
        if (engineName && !nameOk) ++mismatched;
        if (ok) ++answered; else if (obj) ++refused;

        Log("gameopts: id %3d %-32s | menu '%s' | profile %s%ld | want %s | "
            "engine name %s%s | system %s%s",
            e.id, e.psiName, e.menuLabel,
            ok ? "" : "NO ANSWER ", (long)v, e.want,
            engineName ? engineName : "(none)",
            engineName ? (nameOk ? " (matches)" : " MISMATCH - do not believe this row") : "",
            e.sysKey ? e.sysKey : "(no mirror)",
            e.sysKey ? sys : "");
    }

    Log("gameopts: %d/%d ids named by the engine, %d name mismatches, %d "
        "answered, %d refused. A mismatch or a refusal means the PSI table is "
        "wrong for THIS build and the row above it is not evidence.",
        named, kGoCount, mismatched, answered, refused);
    if (!obj)
        Log("gameopts: the profile column is empty for every row because no "
            "profile object was reached - see the REFUSED line above. The "
            "system column is still valid.");
    GoDumpMenuSettings("the automatic read");
    Log("gameopts: ---- end ----");
}

// Ships ON. The workflow here is that the tester plays and the maintainers read
// the log afterwards, so a diagnostic that has to be asked for is one that is
// never asked for. It costs one burst of lines, once, and then nothing.
static void GameOptsConfigure(const char* ini)
{
    g_goAuto = IniFloat(ini, "Diagnostics", "GameOptsOnStart", 1) != 0.0f;
    Log("gameopts: automatic read on gameplay %s ([Diagnostics] GameOptsOnStart)",
        g_goAuto ? "ON" : "off");
}

// VR-161, the lead the decompiled scripts gave: THE MENU HAS ITS OWN SETTING
// IDS, AND ITS OWN SETTER.
//
// `DisGFxMoviePlayerMenuBase` carries
//
//   var array<DisSettingsCategory> m_SettingsCategoryList;
//   native function OnSettingChange(int _SettingID, float _fValue);   0x009F7B80
//   native function OnApplyVideoSettings();                           0x009F93A0
//   native function OnLeaveOptions();                                 0x009F7640
//
// and `DisSettingsCategory` holds `m_SubCategories` and `m_Settings`, each
// `DisSetting` being `{ int m_SettingID; string m_SettingNameOverride; }`.
//
// Two things follow. First, `OnSettingChange` is the same call the options
// screen itself makes, so driving it gets the game's own apply and persistence
// for free rather than us writing a profile array and hoping a consumer
// notices. Second - and this is why every PSI id refused - **the menu's
// `m_SettingID` is not necessarily the PSI enum value.** The category list is
// built at RUNTIME (it is not in defaultproperties and not in any of the 21
// game inis, both checked), so the only way to learn the real ids is to read
// the live list.
//
// This walks it, read-only. It is what turns "the profile array is empty" from
// a dead end into a question with an answer: if the menu enumerates settings
// with ids of its own, the PSI table was the wrong key all along.
static void GoDumpMenuSettings(const char* who)
{
    // ui_state.cpp already watches every live movie player, so reuse its table
    // rather than starting a second scan with its own liveness rules.
    uint8_t* menu = NULL;
    {
        const LONG n = g_uiInstN;
        for (LONG i = 0; i < n && !menu; ++i) {
            uint8_t* o = g_uiInst[i].obj;
            if (!o || !IsLiveObject(o)) continue;
            const char* cn = ObjClassName(o);
            if (cn && strstr(cn, "MoviePlayer") &&
                (strstr(cn, "MenuBase") || strstr(cn, "PauseMenu") || strstr(cn, "MainMenu")))
                menu = o;
        }
    }
    if (!menu) {
        Log("gameopts/menu: no live DisGFxMoviePlayerMenuBase (%s). The settings list belongs to "
            "the menu movie, so open the pause menu once and re-run; this is not a failure.", who);
        return;
    }
    const uint32_t offList = RflOffsetOf("DisGFxMoviePlayerMenuBase", "m_SettingsCategoryList");
    if (!offList) {
        Log("gameopts/menu: m_SettingsCategoryList did not resolve by name on %p", (void*)menu);
        return;
    }
    uint8_t* cats = NULL; int32_t ncat = 0;
    if (!RflArrayAt(menu, offList, &cats, &ncat) || ncat <= 0) {
        Log("gameopts/menu: category list at +0x%x is empty or unreadable (n=%d) on %p - the "
            "options screen has probably not been built yet this session",
            offList, (int)ncat, (void*)menu);
        return;
    }
    Log("gameopts/menu: %d settings categor%s on %p (+0x%x). These ids are what "
        "OnSettingChange takes, and they are NOT assumed to be the PSI enum:",
        (int)ncat, ncat == 1 ? "y" : "ies", (void*)menu, offList);
    // Only the counts and the ids are read here; the nested layout is reported
    // rather than assumed, so a stride that does not match shows as a refusal.
    const uint32_t offSettings = RflOffsetOf("DisSettingsCategory", "m_Settings");
    const uint32_t offId       = RflOffsetOf("DisSetting", "m_SettingID");
    Log("gameopts/menu: DisSettingsCategory.m_Settings +0x%x | DisSetting.m_SettingID +0x%x "
        "(a 0 is UNRESOLVED and the rows below are then not evidence)", offSettings, offId);
    if (!offSettings) {
        Log("gameopts/menu: without m_Settings the categories cannot be walked; nothing below");
        return;
    }
    // The previous build resolved the offsets and then stopped, so it reported
    // "12 categories" and never said what was in them. Walk them.
    //
    // The category stride is not assumed: DisSettingsCategory holds a name, an
    // array of subcategories and an array of settings, and its packed size has
    // not been measured. So this reads the ids at the resolved offset for each
    // category slot and reports how many it could read, and the ids it found.
    // If the stride is wrong the ids will be noise, and a column of values far
    // outside the PSI range (which runs to 153) says so without being asserted.
    const uint32_t catStride = RflOffsetOf("DisSettingsCategory", "m_SubCategories") ? 0 : 0;
    (void)catStride;
    int printed = 0, plausible = 0, wild = 0;
    for (int c = 0; c < ncat && c < 16; ++c) {
        // Each element of an array<struct> is the struct itself, laid out end to
        // end. Without a measured struct size the only honest thing is to read
        // the FIRST category and say so, rather than stride blindly across all.
        if (c > 0) break;
        uint8_t* setsData = NULL; int32_t nsets = 0;
        if (!RflArrayAt(cats, offSettings, &setsData, &nsets) || nsets <= 0 || nsets > 256) {
            Log("gameopts/menu: category 0: m_Settings unreadable or implausible (n=%d). The "
                "DisSettingsCategory stride is not measured, so categories past the first are "
                "deliberately NOT walked - striding on a guess is how a table of nonsense gets "
                "reported as evidence.", (int)nsets);
            break;
        }
        Log("gameopts/menu: category 0 holds %d setting(s):", (int)nsets);
        for (int i = 0; i < nsets && i < 64; ++i) {
            // DisSetting is { int m_SettingID; FString m_SettingNameOverride; }
            // so the id is at +0 of a 16-byte element (int + 12-byte FString).
            // That size is a candidate, not a measurement: the ids it produces
            // are the test.
            const uint8_t* e = setsData + (size_t)i * 16;
            if (!RangeReadable((void*)e, 4)) break;
            const int32_t id = *(const int32_t*)e;
            const bool ok = id >= 0 && id <= 200;
            if (ok) ++plausible; else ++wild;
            Log("gameopts/menu:   [%2d] m_SettingID=%d%s", i, (int)id,
                ok ? "" : "   <- outside the PSI range: the 16-byte element guess is wrong");
            ++printed;
        }
    }
    Log("gameopts/menu: %d id(s) printed, %d plausible, %d outside the PSI range. %s",
        printed, plausible, wild,
        wild > plausible
          ? "MOSTLY WILD: the element size is wrong and none of these ids are evidence."
          : "If these are plausible they are the ids OnSettingChange takes, and they can be "
            "compared against the PSI table to see whether the two schemes agree.");
}

// For the F10 button. Read-only and idempotent, so it just queues.
static void GameOptsRequest(const char* who)
{
    InterlockedExchange(&g_goReq, 1);
    Log("gameopts: read queued by %s", who);
}
static bool GameOptsAutoEnabled() { return g_goAuto; }
static void GameOptsSetAuto(bool on, const char* who)
{
    g_goAuto = on;
    Log("gameopts: automatic read on gameplay %s (%s)", on ? "ON" : "off", who);
}
// True once this session's automatic read has fired, so the panel can say so
// rather than leaving the reader wondering whether it is coming.
static bool GameOptsAutoFired() { return g_goAutoDone; }

// The seam word. Read-only, so it needs no confirmation and no A/B toggle.
static bool GameOptsCommand(const char* args)
{
    if (!args || !args[0] || !strcmp(args, "read") || !strcmp(args, "status")) {
        InterlockedExchange(&g_goReq, 1);
        Log("gameopts: queued a read for the script lane (profile + system). "
            "Needs GAMEPLAY - the profile object comes off the player controller.");
        return true;
    }
    if (!strcmp(args, "system")) {
        InterlockedExchange(&g_goReq, 2);
        Log("gameopts: queued a SystemSettings-only read for the script lane");
        return true;
    }
    Log("gameopts: usage - gameopts [read|system]. Read-only: it reports the "
        "game's own option settings and never writes one.");
    return true;
}
