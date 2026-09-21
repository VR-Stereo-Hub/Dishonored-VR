// game/dishonored/cam_modifiers.cpp - VR-165: WHICH camera modifier is still
// swinging? Read-only. Included by src/mod/dishonoredvr.cpp after
// ue3/reflect.cpp, whose name-keyed resolver this uses.
//
// THE MEASUREMENT THIS EXISTS TO SETTLE. A playtester came off a chain and the
// camera went on swinging as if still on it, for about 23 seconds. Measured
// from their log: the engine's player state machine was clean
// (Climb -> Jump -> Falling -> Walk), OUR positional request was flat in every
// window (0-1 reversals, under 5 uu of range), and the GAME's own camera
// oscillated at ~8 Hz with up to 413 uu of range - five times the amplitude of
// ordinary walking. So the swing is game-side and no clamp on our own writer
// can touch it.
//
// Camera.ModifierList excludes only UE3 CameraModifier objects. Dishonored uses
// a SEPARATE m_InfluenceGroups array. An empty ModifierList never eliminated
// those influences; the source probe below reads that actual graph.
// The old frequency estimates were retracted; see FLICKER_REFERENCE.md.
//
// The probe is kept because that elimination is the result, and because it is
// what will show a modifier arriving if one ever does.
//
// HOW IT READS. `Camera.ModifierList` is a UE3 `array<CameraModifier>`, and
// each `CameraModifier` carries Alpha, TargetAlpha, Priority and bDisabled.
// Every offset comes from the name-keyed resolver (RflOffsetOf), never from a
// hardcoded number: a name outlives a game rebuild and this project has paid
// for copied offsets before. A property that cannot be resolved is reported as
// unresolved on the line rather than read as a zero.
//
// It writes nothing. It retains no pointer across frames.

namespace {

// Ships ON: the tester plays in a headset and cannot reach a prompt, so a probe
// that has to be asked for is one that never runs. [Diagnostics] CamModProbe.
bool     g_cmOn        = true;
double   g_cmNextMs    = 0.0;
double   g_cmLastMoveMs= 0.0;   // when the camera last moved a lot (the swing)
float    g_cmLastZ     = 0.0f;
bool     g_cmHaveZ     = false;
int      g_cmSwingRuns = 0;     // consecutive fast reversals seen
float    g_cmExtreme   = 0.0f;   // running extreme of the current leg
int      g_cmLeg       = 0;      // -1 falling, +1 rising, 0 unknown
double   g_cmBeatMs    = 0.0;    // the watcher's own heartbeat
double   g_cmPeakHz    = 0.0;
float    g_cmPeakAmp   = 0.0f;
bool     g_cmPrinted   = false;  // has any table printed this session?
int      g_cmReversals = 0;
double   g_cmWindowMs  = 0.0;
float    g_cmZMin=0.0f, g_cmZMax=0.0f;

const int   kCmPeriodMs = 1000;   // one table per second while it has something to say
// A direction change only counts once the camera has actually travelled this
// far back from the leg's extreme. Below this it is sampling noise, not motion.
const float kCmTurnUu   = 2.0f;

// Offsets, resolved once by NAME. 0 = unresolved, and the line says so.
uint32_t g_cmOffList=0, g_cmOffAlpha=0, g_cmOffTarget=0, g_cmOffPrio=0;
uint32_t g_cmOffDisabled=0, g_cmMaskDisabled=0;
bool     g_cmResolved=false;

void CmResolve()
{
    if (g_cmResolved) return;
    g_cmResolved = true;
    g_cmOffList   = RflOffsetOf("Camera", "ModifierList");
    g_cmOffAlpha  = RflOffsetOf("CameraModifier", "Alpha");
    g_cmOffTarget = RflOffsetOf("CameraModifier", "TargetAlpha");
    g_cmOffPrio   = RflOffsetOf("CameraModifier", "Priority");
    FindBoolProp("CameraModifier", "bDisabled", &g_cmOffDisabled, &g_cmMaskDisabled);
    Log("cammod: resolved by name - Camera.ModifierList +0x%x | CameraModifier Alpha +0x%x "
        "TargetAlpha +0x%x Priority +0x%x bDisabled +0x%x/0x%x. A 0 here is UNRESOLVED, "
        "not an offset of zero, and every row below will say so.",
        g_cmOffList, g_cmOffAlpha, g_cmOffTarget, g_cmOffPrio, g_cmOffDisabled, g_cmMaskDisabled);
}

// The camera's Z, for correlating a modifier's weight against the actual
// swing. This reads the SAME name-resolved CameraCache POV location that
// cinematic_trace prints as `cachePos`, so the number in this table and the
// number the oscillation was measured from are the same quantity. No new
// offset is introduced here; if the trace has not resolved its layout yet,
// this reports no Z rather than reading a guessed address.
bool CmCameraZ(uint8_t* cam, float* z)
{
    if (!cam || !g_ctLayout || !g_ctCache) return false;
    float loc[3] = {};
    if (!CtRead(cam, g_ctCache + g_ctPov + g_ctLoc, loc, sizeof(loc))) return false;
    *z = loc[2];
    return true;
}

} // namespace

// Sample the real source graph on the script lane, never from Present.
// Includes healthy samples and unresolved reads; no threshold can hide a result.
static void CameraSourceTick()
{
    if (!g_cmOn) return;
    static double next=0;
    static unsigned samples=0;
    const double now=MaimNowMs();
    if(now<next || samples>=1800) return;
    next=now+500;
    uint8_t* cam=g_camObj; uint8_t* pawn=g_pePawn;
    if(!cam || !pawn) return;
    ++samples;
    if(!BuildLiveSet() || !IsLiveObject(cam) || !IsLiveObject(pawn)) {
        Log("camera/source: sample=%u unavailable: live camera/pawn validation failed",samples); return;
    }
    auto scalar = [](uint8_t* o,const char* cls,const char* prop) -> float {
        float v=NAN; const uint32_t off=RflOffsetOf(cls,prop);
        if(o && off && RangeReadable(o+off,4)) memcpy(&v,o+off,4);
        return v;
    };
    auto vector = [](uint8_t* o,const char* cls,const char* prop,float* v) {
        v[0]=v[1]=v[2]=NAN; const uint32_t off=RflOffsetOf(cls,prop);
        if(!o || !off || !RangeReadable(o+off,12)) return false;
        memcpy(v,o+off,12);
        return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
    };
    float pov[3]={NAN,NAN,NAN},loc[3],vel[3];
    const bool povRead=g_ctLayout && g_ctCache && CtRead(cam,g_ctCache+g_ctPov+g_ctLoc,pov,sizeof(pov));
    const bool povOk=povRead && std::isfinite(pov[0]) && std::isfinite(pov[1]) && std::isfinite(pov[2]);
    const bool locOk=vector(pawn,"Actor","Location",loc);
    vector(pawn,"Actor","Velocity",vel);
    const bool ownerOk=IsLiveObject(g_peCtrl) && g_ctPcCamera && g_ctPawn &&
        CtObject(g_peCtrl,g_ctPcCamera)==cam && CtObject(g_peCtrl,g_ctPawn)==pawn;
    const bool deltaOk=povOk && locOk && ownerOk;
    float delta[3]={NAN,NAN,NAN};
    if(deltaOk) for(int k=0;k<3;++k) delta[k]=pov[k]-loc[k];
    const dvr::anim::Snapshot state=dvr::anim::snapshot();
    Log("camera/source: sample=%u time=%.0f cam=%p pawn=%p state=%s owner-match=%d "
        "pov-read=%d pawn-read=%d camera-world=%.2f/%.2f/%.2f pawn-world=%.2f/%.2f/%.2f "
        "eye-delta=%s %.2f/%.2f/%.2f velocity=%.2f/%.2f/%.2f EyeHeight=%.2f BaseEyeHeight=%.2f; "
        "sequential cache snapshot, freshness unverified; 500ms samples are not frequency evidence",
        samples,now,cam,pawn,state.state[0],int(ownerOk),int(povOk),int(locOk),
        pov[0],pov[1],pov[2],loc[0],loc[1],loc[2],deltaOk?"computed":"UNAVAILABLE",
        delta[0],delta[1],delta[2],vel[0],vel[1],vel[2],
        scalar(pawn,"Pawn","EyeHeight"),scalar(pawn,"Pawn","BaseEyeHeight"));
    uint8_t* groups=NULL; int32_t count=0;
    const uint32_t off=RflOffsetOf("DishonoredPlayerCamera","m_InfluenceGroups");
    if(!off || !RflArrayAt(cam,off,&groups,&count) || count>16 ||
        (count && !RangeReadable(groups,count*sizeof(void*)))) {
        Log("camera/source: influence groups unavailable count=%d",count); return;
    }
    Log("camera/source: groups=%d (separate from Camera.ModifierList)",count);
    for(int g=0;g<count;++g) {
        uint8_t* group=((uint8_t**)groups)[g];
        if(!group) { Log("camera/source: sample=%u group=%d obj=%p EMPTY null slot",samples,g,group); continue; }
        if(!IsLiveObject(group)) { Log("camera/source: sample=%u group=%d obj=%p REJECTED non-null pointer not live",samples,g,group); continue; }
        uint8_t* influences=NULL; int32_t n=0;
        const uint32_t io=RflOffsetOf("DishonoredCameraInfluenceGroup","m_Influences");
        if(!io || !RflArrayAt(group,io,&influences,&n) || n>64 ||
           (n && !RangeReadable(influences,n*sizeof(void*)))) {
            Log("camera/source: group=%d influences unavailable n=%d",g,n); continue;
        }
        Log("camera/source: group=%d obj=%p weight=%g influences=%d",g,group,
            scalar(group,"DishonoredCameraInfluenceGroup","m_Weight"),n);
        for(int i=0;i<n;++i) {
            uint8_t* inf=((uint8_t**)influences)[i];
            if(!IsLiveObject(inf)) { Log("camera/source: group=%d row=%d not live",g,i); continue; }
            const char* cls=ObjClassName(inf);
            float source[3]={NAN,NAN,NAN}; const char* field="not exposed";
            if(cls && (!strcmp(cls,"DishonoredCamera_PlayerControl") || !strcmp(cls,"DishonoredCamera_AnimDriven"))) field="m_Debug_POV_Location";
            else if(cls && !strcmp(cls,"DishonoredCamera_CrouchMantleOffset")) field="m_StartingOffset";
            else if(cls && !strcmp(cls,"DisCamera_StepUpMantleOffset")) field="m_StepUpStartPos";
            const bool sourceOk=strcmp(field,"not exposed") && vector(inf,cls,field,source);
            const bool worldPov=!strcmp(field,"m_Debug_POV_Location");
            float relative[3]={NAN,NAN,NAN};
            const bool relativeOk=worldPov && sourceOk && locOk && ownerOk;
            if(relativeOk) for(int k=0;k<3;++k) relative[k]=source[k]-loc[k];
            if(worldPov) Log("camera/source: sample=%u group=%d row=%d source-world=%.2f/%.2f/%.2f "
                "source-minus-pawn=%s %.2f/%.2f/%.2f (debug field freshness unverified)",
                samples,g,i,source[0],source[1],source[2],relativeOk?"computed":"UNAVAILABLE",
                relative[0],relative[1],relative[2]);
            Log("camera/source: group=%d row=%d obj=%p class=%s weight=%g target=%g %s=%.2f/%.2f/%.2f",
                g,i,inf,cls?cls:"unavailable",scalar(inf,"DishonoredCameraInfluence","m_Weight"),
                scalar(inf,"DishonoredCameraInfluence","m_TargetWeight"),field,source[0],source[1],source[2]);
        }
    }
    if(samples==1800) Log("camera/source: sample budget exhausted; restart required for more source snapshots");
}

// Called from the script lane next to the other per-tick readers.
static void CamModTick()
{
    CameraSourceTick();
    if (!g_cmOn) return;
    uint8_t* cam = g_camObj;
    if (!cam || !IsLiveObject(cam)) return;
    CmResolve();
    if (!g_cmOffList) return;      // nothing to walk; CmResolve already said so

    const double now = MaimNowMs();

    // Track the swing itself, so the table can be printed WITH the thing it is
    // meant to explain rather than next to it. A reversal only counts when the
    // step is big enough to be motion rather than noise.
    // CORRECTED after run 2. This used to count a reversal per ProcessEvent
    // tick, and ProcessEvent runs far faster than any camera motion - so it
    // reported "249 reversals in 1010 ms", which is not a frequency of
    // anything and cannot be compared against the ~8 Hz the offline analysis
    // measured. A counter whose units are "per dispatch" is not evidence about
    // motion.
    //
    // Count DIRECTION CHANGES OF A REAL EXCURSION instead: a reversal is only
    // counted once the camera has travelled kCmTurnUu since the last one, so
    // the number is half-cycles of actual movement and dividing by the window
    // gives a rate in Hz that means something. Sampling rate no longer changes
    // the answer.
    float z = 0.0f;
    if (CmCameraZ(cam, &z)) {
        if (g_cmHaveZ) {
            // REGRESSION FIXED. The first version of this updated g_cmExtreme
            // toward z on EVERY sample, in whichever direction z had just
            // moved. That made the extreme track z, so `z - extreme` was always
            // about zero and could never cross the threshold: the detector
            // counted nothing and the probe printed no table at all during a
            // reproduction the user confirmed. A counter that cannot fire is
            // worse than the noisy one it replaced, and this one shipped
            // untested because the swing did not occur on the run after it.
            //
            // The extreme belongs to the LEG, and only ever moves further OUT:
            // on a rising leg it is the highest z seen, on a falling leg the
            // lowest. A turn is z retreating from it by more than kCmTurnUu.
            if (g_cmLeg > 0) { if (z > g_cmExtreme) g_cmExtreme = z; }
            else if (g_cmLeg < 0) { if (z < g_cmExtreme) g_cmExtreme = z; }
            else {
                // No leg yet: adopt one as soon as there is real movement.
                if (z > g_cmExtreme + kCmTurnUu) { g_cmLeg = 1; g_cmExtreme = z; }
                else if (z < g_cmExtreme - kCmTurnUu) { g_cmLeg = -1; g_cmExtreme = z; }
            }
            if (g_cmLeg > 0 && z < g_cmExtreme - kCmTurnUu) {
                ++g_cmReversals; g_cmLeg = -1; g_cmExtreme = z;
            } else if (g_cmLeg < 0 && z > g_cmExtreme + kCmTurnUu) {
                ++g_cmReversals; g_cmLeg = 1; g_cmExtreme = z;
            }
            if (z < g_cmZMin) g_cmZMin = z;
            if (z > g_cmZMax) g_cmZMax = z;
        } else { g_cmZMin = g_cmZMax = g_cmExtreme = z; g_cmWindowMs = now; g_cmLeg = 0; }
        g_cmLastZ = z; g_cmHaveZ = true;
    }

    if (now < g_cmNextMs) return;
    g_cmNextMs = now + kCmPeriodMs;

    const double win = now - g_cmWindowMs;
    const float  amp = g_cmZMax - g_cmZMin;
    const int    rev = g_cmReversals;
    // Reset the window for the next second.
    g_cmReversals = 0; g_cmWindowMs = now; g_cmZMin = g_cmZMax = g_cmLastZ;

    // ~8 Hz with tens of uu was the measured chain swing; walking was far below
    // it. Print while swinging, and once more when it stops, so the release is
    // visible too - the release is the half that would identify an owner.
    const double hz = win > 0.0 ? (rev * 0.5) / (win / 1000.0) : 0.0;
    const bool swinging = (hz >= 2.0 && amp > 15.0f);
    if (swinging) { g_cmLastMoveMs = now; ++g_cmSwingRuns; }
    const bool justStopped = !swinging && g_cmSwingRuns > 0 && (now - g_cmLastMoveMs) < 3000.0;
    // The detector must be able to say it is alive. Its first version counted
    // nothing and printed nothing, and "no tables" read exactly like "no
    // swing" - so a broken instrument looked like a clean run. This line makes
    // silence explainable: it reports what the watcher is actually seeing, and
    // the peak it has seen, so a threshold that is never reached is visible
    // rather than invisible.
    if (g_cmPeakHz < hz) g_cmPeakHz = hz;
    if (g_cmPeakAmp < amp) g_cmPeakAmp = amp;
    if (now >= g_cmBeatMs) {
        g_cmBeatMs = now + 30000.0;
        Log("cammod: watching - this window %.1f Hz / %.1f uu; peak since the last beat "
            "%.1f Hz / %.1f uu; a table prints at >= 2.0 Hz AND > 15 uu. %s",
            hz, amp, g_cmPeakHz, g_cmPeakAmp,
            g_cmPrinted ? "Tables have printed this session."
                        : "NO table has printed yet this session.");
        g_cmPeakHz = 0.0; g_cmPeakAmp = 0.0f;
    }
    if (!swinging && !justStopped) { if (!swinging) g_cmSwingRuns = 0; return; }
    g_cmPrinted = true;
    if (!swinging) g_cmSwingRuns = 0;

    uint8_t* data = NULL; int32_t num = 0;
    if (!RflArrayAt(cam, g_cmOffList, &data, &num) || !data || num <= 0 || num > 64) {
        Log("cammod: %s - ModifierList unreadable or empty (num=%d) on camera %p; "
            "the swing cannot be attributed to a modifier from here",
            swinging ? "SWINGING" : "settled", (int)num, (void*)cam);
        return;
    }

    Log("cammod: ---- %s: %.1f Hz (%d turns of >%.0f uu in %.0f ms), %.1f uu of camera Z, %d modifier(s) ----",
        swinging ? "SWINGING" : "SETTLED (the swing just stopped)",
        win > 0.0 ? (rev * 0.5) / (win / 1000.0) : 0.0, rev, kCmTurnUu, win, amp, (int)num);
    for (int i = 0; i < num; ++i) {
        uint8_t* m = *(uint8_t**)(data + i * sizeof(void*));
        if (!m || ((uintptr_t)m & 3) || !RangeReadable(m, kClassOff + 4)) {
            Log("cammod:   [%d] unreadable entry %p", i, (void*)m);
            continue;
        }
        const char* cn = ObjClassName(m);
        char alpha[24] = "unresolved", targ[24] = "unresolved", prio[16] = "unresolved";
        char dis[16] = "unresolved";
        if (g_cmOffAlpha  && RangeReadable(m + g_cmOffAlpha, 4))
            _snprintf(alpha, sizeof(alpha), "%.3f", *(const float*)(m + g_cmOffAlpha));
        if (g_cmOffTarget && RangeReadable(m + g_cmOffTarget, 4))
            _snprintf(targ, sizeof(targ), "%.3f", *(const float*)(m + g_cmOffTarget));
        if (g_cmOffPrio   && RangeReadable(m + g_cmOffPrio, 1))
            _snprintf(prio, sizeof(prio), "%u", (unsigned)*(const uint8_t*)(m + g_cmOffPrio));
        if (g_cmOffDisabled && g_cmMaskDisabled && RangeReadable(m + g_cmOffDisabled, 4))
            _snprintf(dis, sizeof(dis), "%d",
                      (*(const uint32_t*)(m + g_cmOffDisabled) & g_cmMaskDisabled) ? 1 : 0);
        Log("cammod:   [%d] %-44s alpha=%s target=%s prio=%s disabled=%s",
            i, cn ? cn : "(no class name)", alpha, targ, prio, dis);
    }
    Log("cammod: ---- end. A modifier whose alpha stays UP on the SETTLED line is not the "
        "owner; the owner is one that is still up while SWINGING and falls when it settles. "
        "Every alpha reading 'unresolved' means read the resolver line above, not this table. ----");
}

static void CamModConfigure(const char* ini)
{
    g_cmOn = IniFloat(ini, "Diagnostics", "CamModProbe", 1) != 0.0f;
    Log("cammod: camera-modifier probe %s ([Diagnostics] CamModProbe) - read-only, prints only "
        "while the camera is actually swinging", g_cmOn ? "ON" : "off");
}

static bool CamModCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) {
        g_cmOn = b;
        ConfigWriteKey("Diagnostics", "CamModProbe", b ? "1" : "0", "the seam");
        Log("cammod: %s (seam)", b ? "ON" : "off");
        return true;
    }
    Log("cammod: on|off (now %s). Read-only: it names which camera modifier is still "
        "weighted while the camera swings, and writes nothing.", g_cmOn ? "ON" : "off");
    return true;
}
