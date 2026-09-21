// game/dishonored/swing_trace.cpp - VR-165: the RAW camera series, sampled at
// present rate. Read-only. Included after camera.cpp (it calls
// dvr::camera::render_pos_world) and before commands.cpp.
//
// WHY A RAW SERIES AND NOT ANOTHER COUNTER. Two instruments have now produced
// a frequency for this bug and BOTH were artifacts of their own sampling:
//
//   - the offline analysis read `cachePos` out of cine/trace, which is logged
//     about every 109 ms. That is ~9 Hz, so it cannot resolve an 8 Hz
//     oscillation at all; "~8 Hz" was aliasing, and it reached a ticket.
//   - cam_modifiers' own counter samples on the ProcessEvent lane and reported
//     ~110 Hz, which is approximately the ProcessEvent rate. It was measuring
//     itself.
//
// So this derives nothing. It captures the camera position the player actually
// sees, once per present, into a fixed ring, and when an excursion trips it
// dumps the SAMPLES - timestamps and values - for offline analysis where
// aliasing is visible in the spacing. A number computed inside a broken
// sampler is worse than no number, and this project has now paid for that
// twice in one day.
//
// The signal is `dvr::camera::render_pos_world`, which is the render camera and
// is already read on the present thread by the eyetest, so no new thread
// contract is introduced and no engine pointer is retained.

namespace {

// One present's worth. Scalars only.
struct SwSample { double ms; float x, y, z; };

const int kSwRing = 2048;          // ~14 s at 144 Hz, ~20 s at 100
SwSample  g_swRing[kSwRing];
int       g_swHead = 0;            // next write index
int       g_swCount = 0;           // how many are valid
bool      g_swOn = true;           // [Diagnostics] SwingTrace
double    g_swLastDumpMs = 0.0;
int       g_swDumps = 0;
double    g_swBeatMs = 0.0;

// A dump costs a burst of lines, so it is rate limited and capped for a run.
const int    kSwMaxDumps    = 6;
const double kSwDumpGapMs   = 20000.0;
const int    kSwDumpSamples = 256;   // the most recent N, oldest first
// The excursion that arms a dump, judged over the ring's recent span. Chosen
// well above ordinary walking (about 50 uu of Z over a second, measured) and
// below the reported swing (hundreds of uu).
const float  kSwArmUu       = 120.0f;
const double kSwArmWindowMs = 1500.0;

} // namespace

// Present thread, once per present. Must stay cheap: three floats and an index.
static void SwingTracePresentTick()
{
    if (!g_swOn) return;
    float p[3];
    if (!dvr::camera::render_pos_world(p)) return;
    const double now = MaimNowMs();
    SwSample& s = g_swRing[g_swHead];
    s.ms = now; s.x = p[0]; s.y = p[1]; s.z = p[2];
    g_swHead = (g_swHead + 1) % kSwRing;
    if (g_swCount < kSwRing) ++g_swCount;

    // VR-165: the arc test rides the same present-rate sample. g_hmdPitch is
    // the head pitch the camera seam already uses, in radians.
    SwingArcWatch(p[0], p[1], p[2], g_hmdPitch * 57.29578f);

    // Arm on a big recent excursion in Z, which is what the reports describe
    // (being moved up and down / back and forth). No frequency is computed.
    if (g_swCount < 32) return;
    float zmin = 1e30f, zmax = -1e30f, xmin = 1e30f, xmax = -1e30f;
    int n = 0;
    for (int i = 1; i <= g_swCount; ++i) {
        const SwSample& q = g_swRing[(g_swHead - i + kSwRing) % kSwRing];
        if (now - q.ms > kSwArmWindowMs) break;
        if (q.z < zmin) zmin = q.z;
        if (q.z > zmax) zmax = q.z;
        if (q.x < xmin) xmin = q.x;
        if (q.x > xmax) xmax = q.x;
        ++n;
    }
    if (n < 16) return;

    if (now >= g_swBeatMs) {
        g_swBeatMs = now + 30000.0;
        Log("swing: watching at present rate - last %.1f s: %d samples, Z span %.1f uu, X span "
            "%.1f uu (a dump arms at %.0f uu of Z). Dumps so far %d of %d. This instrument "
            "derives NO frequency; it prints samples.",
            kSwArmWindowMs / 1000.0, n, zmax - zmin, xmax - xmin, kSwArmUu, g_swDumps, kSwMaxDumps);
    }

    if (zmax - zmin < kSwArmUu) return;
    if (g_swDumps >= kSwMaxDumps) return;
    if (now - g_swLastDumpMs < kSwDumpGapMs) return;
    g_swLastDumpMs = now; ++g_swDumps;

    const int want = g_swCount < kSwDumpSamples ? g_swCount : kSwDumpSamples;
    Log("swing: ---- RAW SERIES %d of %d: %d samples at present rate, Z span %.1f uu over the "
        "last %.1f s. Columns are ms,x,y,z. Read the SPACING before reading the shape - if the "
        "spacing is near the period, the shape is aliased and means nothing. ----",
        g_swDumps, kSwMaxDumps, want, zmax - zmin, kSwArmWindowMs / 1000.0);
    // Oldest first, batched so one line carries several samples.
    char line[900]; int used = 0; int inLine = 0;
    for (int i = want; i >= 1; --i) {
        const SwSample& q = g_swRing[(g_swHead - i + kSwRing) % kSwRing];
        const int wrote = _snprintf(line + used, sizeof(line) - used, "%s%.0f,%.1f,%.1f,%.1f",
                                    inLine ? " " : "", q.ms, q.x, q.y, q.z);
        if (wrote <= 0 || used + wrote >= (int)sizeof(line) - 48) {
            line[used] = 0;
            Log("swing:   %s", line);
            used = 0; inLine = 0;
            const int again = _snprintf(line, sizeof(line), "%.0f,%.1f,%.1f,%.1f",
                                        q.ms, q.x, q.y, q.z);
            used = again > 0 ? again : 0; inLine = 1;
            continue;
        }
        used += wrote; ++inLine;
    }
    if (used > 0) { line[used] = 0; Log("swing:   %s", line); }
    Log("swing: ---- end of series %d ----", g_swDumps);
}

// VR-165: DID THE PLAYER ASK FOR THAT CLIMB, OR DID THE GAME RE-GRAB BY ITSELF?
//
// A run showed five Climb -> Falling -> Walk cycles in ten seconds, every
// Falling lasting 0.0 s, which looked exactly like "constantly trying to fall
// and not succeeding". It was very nearly reported as the bug's signature. The
// user then pointed out that they had deliberately grabbed and released the
// chain several times to provoke it - so that sequence is indistinguishable
// from ordinary input, and reporting it would have been reporting the tester's
// own hands back at them.
//
// The discriminator is therefore not the cycle. It is whether a Climb entry was
// PRECEDED BY A BUTTON. A climb the player asked for has a press within the
// preceding window; one the game started on its own does not. That is the line
// between "the tester was testing" and "the bug re-armed itself", and nothing
// else in the log draws it.
//
// Read-only: it samples the pad state the mod already synthesises, and the
// master state the script lane already reads. It writes neither.
static void SwingClimbWatch(const char* masterState)
{
    if (!g_swOn || !masterState) return;
    const double now = MaimNowMs();
    const bool climbing = strstr(masterState, "Climb") != NULL;

    // Any face button or trigger counts as "the player did something". The
    // grab is a button in every scheme, so a press within the window is enough
    // to explain a climb without claiming which button it was.
    // THE TEST MUST BE AUDITABLE. The first version printed "ON A PRESS - 0 ms
    // ago" for all five climbs in a run, which is the answer it would give if
    // the mask were NEVER zero - a held button, or a mask that does not clear.
    // Nothing else in the log could tell those apart, so the result was
    // worthless in exactly the way this project keeps rediscovering: an
    // instrument that cannot print the unwelcome answer is not evidence.
    //
    // So it now tracks how long the mask has been continuously non-zero, and
    // reports the mask itself. "Asked" only counts when the mask went zero
    // recently enough for the press to be a distinct event; a mask that has
    // been held for seconds is reported as UNAUDITABLE rather than as a press.
    static double lastPressMs = 0.0, lastZeroMs = 0.0;
    static LONG   lastMask = 0;
    const LONG mask = InterlockedCompareExchange(&g_padBtnsPub, 0, 0);
    if (mask != 0) lastPressMs = now; else lastZeroMs = now;
    lastMask = mask;

    static bool wasClimbing = false;
    static int  unaskedRun = 0, askedRun = 0;
    if (climbing && !wasClimbing) {
        const double sincePress = lastPressMs > 0.0 ? now - lastPressMs : -1.0;
        const double heldFor    = lastZeroMs > 0.0 ? now - lastZeroMs : -1.0;
        // A press only explains the climb if the pad was actually idle at some
        // point in the last couple of seconds. Otherwise we cannot distinguish
        // "pressed to grab" from "mask never clears".
        const bool auditable = lastZeroMs > 0.0 && heldFor < 2000.0;
        const bool asked     = auditable && sincePress >= 0.0 && sincePress < 600.0;
        if (!auditable) { /* neither run advances: the test did not decide */ }
        else if (asked) { ++askedRun; unaskedRun = 0; }
        else            { ++unaskedRun; askedRun = 0; }
        Log("swing/climb: entered Climb - mask=0x%04x, last non-zero %.0f ms ago, pad last IDLE "
            "%.0f ms ago -> %s (asked-in-a-row %d, unasked-in-a-row %d)",
            (unsigned)mask, sincePress, heldFor,
            !auditable ? "UNAUDITABLE: the pad mask has not been idle recently, so a press "
                         "cannot be told from a stuck mask and this climb decides nothing"
                       : asked ? "ASKED: player input explains this"
                               : "UNASKED: NOTHING THE PLAYER DID EXPLAINS THIS",
            askedRun, unaskedRun);
        if (unaskedRun >= 2)
            Log("swing/climb: %d consecutive AUDITABLE climbs with no button behind them - this "
                "is the signature to chase, and it is NOT the tester grabbing repeatedly",
                unaskedRun);
    }
    wasClimbing = climbing;
}

// VR-165: DOES THE EYE TRAVEL ON AN ARC WHEN THE HEAD PITCHES?
//
// The user reported that in the bugged state looking up sends the view into the
// sky and looking down does the reverse - a huge swivel. Two measurements then
// said it is not what it sounds like: head pitch spans 165.6 deg while camera
// pitch spans 162.6, a ratio of 0.98, so the ANGLE is tracked almost exactly
// 1:1 and nothing is amplifying it; and the neck pivot is constant at
// 0.321/0.062 across 454 samples, so the modelled lever arm is not growing.
//
// A camera that rotates correctly but TRANSLATES on a long arc produces exactly
// that sensation, and it also explains "I feel taller", which has survived
// every measurement so far. So the question is no longer the angle - it is
// whether the eye position moves as a function of pitch, and with what radius.
//
// This correlates the render camera's position against head pitch over a short
// window. No radius or ownership is inferred from these spans.
// Report observed spans only. Independent axis extrema do not define a radius.
static void SwingArcWatch(float camX, float camY, float camZ, float headPitchDeg)
{
    if (!g_swOn) return;
    const double now = MaimNowMs();
    static double start = 0;
    static float lo[4], hi[4];
    static int n = 0;
    const float v[4] = {camX, camY, camZ, headPitchDeg};
    for (int i=0; i<4; ++i) if (!std::isfinite(v[i])) return;
    if (!n) { start=now; for(int i=0;i<4;++i) lo[i]=hi[i]=v[i]; }
    for(int i=0;i<4;++i) { if(v[i]<lo[i]) lo[i]=v[i]; if(v[i]>hi[i]) hi[i]=v[i]; }
    ++n;
    if(now-start<2000) return;
    Log("swing/arc: samples=%d elapsed=%.0fms xyz-span=%.1f/%.1f/%.1f pitch-span=%.1f deg; "
        "spans include locomotion and angle wrapping; no radius or cause inferred", n, now-start,
        hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2],hi[3]-lo[3]);
    n=0;
}

static void SwingTraceConfigure(const char* ini)
{
    g_swOn = IniFloat(ini, "Diagnostics", "SwingTrace", 1) != 0.0f;
    Log("swing: raw present-rate camera trace %s ([Diagnostics] SwingTrace). It samples "
        "render_pos_world every present and dumps SAMPLES, never a derived rate - two earlier "
        "instruments reported frequencies that were their own sampling rate.",
        g_swOn ? "ON" : "off");
}

static bool SwingTraceCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) {
        g_swOn = b;
        ConfigWriteKey("Diagnostics", "SwingTrace", b ? "1" : "0", "the seam");
        Log("swing: trace %s (seam: swingtrace)", b ? "ON" : "off");
        return true;
    }
    Log("swing: swingtrace on|off (now %s). %d of %d dumps used. Read-only.",
        g_swOn ? "ON" : "off", g_swDumps, kSwMaxDumps);
    return true;
}
