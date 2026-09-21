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
// window and reports the implied radius: how far the eye moves per radian of
// pitch. Standing still and looking around, a correct camera moves by roughly
// the neck offset (a few uu). A number in the hundreds is the arc the user is
// describing, and its size names the offset that is wrong.
//
// Read-only. It derives a radius from measured spans and says what would make
// it meaningless, rather than asserting one.
static void SwingArcWatch(float camX, float camY, float camZ, float headPitchDeg)
{
    if (!g_swOn) return;
    const double now = MaimNowMs();
    static double winStart = 0.0;
    static float pMin = 1e30f, pMax = -1e30f;
    static float xMin = 1e30f, xMax = -1e30f, yMin = 1e30f, yMax = -1e30f, zMin = 1e30f, zMax = -1e30f;
    static int   n = 0;

    if (winStart == 0.0) winStart = now;
    if (headPitchDeg < pMin) pMin = headPitchDeg;
    if (headPitchDeg > pMax) pMax = headPitchDeg;
    if (camX < xMin) xMin = camX; if (camX > xMax) xMax = camX;
    if (camY < yMin) yMin = camY; if (camY > yMax) yMax = camY;
    if (camZ < zMin) zMin = camZ; if (camZ > zMax) zMax = camZ;
    ++n;

    if (now - winStart < 2000.0) return;
    const float pitchSpanDeg = pMax - pMin;
    const float posSpan = sqrtf((xMax - xMin) * (xMax - xMin) +
                                (yMax - yMin) * (yMax - yMin) +
                                (zMax - zMin) * (zMax - zMin));
    const float pitchRad = pitchSpanDeg * 0.0174533f;
    // Only meaningful when the head actually pitched; otherwise the ratio is
    // position change divided by nothing, which is how a previous instrument in
    // this same file produced a number that meant nothing.
    if (n >= 30 && pitchSpanDeg >= 20.0f) {
        const float radiusUu = posSpan / (pitchRad > 0.01f ? pitchRad : 1.0f);
        if (radiusUu > 60.0f) {
            Log("swing/arc: the eye moved %.1f uu while the head pitched %.1f deg -> implied "
                "radius %.0f uu per radian. A correct camera moves about the neck offset (a few "
                "uu); this is an ARC, and its radius is the offset that is wrong. Walking also "
                "moves the eye, so treat this as a lead only if the player was standing still.",
                posSpan, pitchSpanDeg, radiusUu);
            // WHOSE camera is this? Our own contribution is already known to be
            // small (max 62 uu against a 350 uu arc), so the arc is the GAME
            // moving its own camera - but nothing yet says whether it switched
            // to a different camera object or mode when the chain was released.
            // cine/trace carries that identity and fired six times in a whole
            // run, so it was never available when it mattered. Print it HERE,
            // beside the arc that needs explaining, rather than hoping another
            // subsystem happens to log in the same second.
            uint8_t* cam = g_camObj;
            const char* cls = (cam && IsLiveObject(cam)) ? ObjClassName(cam) : NULL;
            float gamePos[3] = {0,0,0};
            const bool gameOk = cam && g_ctLayout && g_ctCache &&
                                CtRead(cam, g_ctCache + g_ctPov + g_ctLoc, gamePos, sizeof(gamePos));
            float ours[3] = {0,0,0};
            dvr::camera::position_offset_uu(ours);
            const float ourMag = std::sqrt(ours[0]*ours[0] + ours[1]*ours[1] + ours[2]*ours[2]);
            Log("swing/arc:   camera object %p class '%s' | game POV %s%.1f/%.1f/%.1f | "
                "OUR offset %.1f uu | eye %.1f/%.1f/%.1f",
                (void*)cam, cls ? cls : "(unreadable)",
                gameOk ? "" : "UNREADABLE ", gamePos[0], gamePos[1], gamePos[2],
                ourMag, camX, camY, camZ);
            // THE NUMBER THAT NAMES IT. Run 9 settled the remaining doubt: the
            // camera object and class never change, and the player's velocity
            // was 0.0 in every arc window, so this is not a camera swap and not
            // locomotion - the game's own POV translates hundreds of uu while
            // the pawn stands still.
            //
            // A POV that moves while its pawn does not is an OFFSET being
            // rotated. In first person that offset should be about eye height
            // (the collision cylinder is 87.5 uu, so roughly 60-70 uu); an
            // offset of several hundred is a lever long enough to produce the
            // measured arc. So print POV minus pawn: its magnitude IS the lever
            // arm, and comparing it against a healthy run says whether the
            // offset grew or was always this size and only started rotating.
            if (gameOk && g_pePawn && g_actorLocFound &&
                RangeReadable(g_pePawn + g_actorLocOff, 12)) {
                const float* pw = (const float*)(g_pePawn + g_actorLocOff);
                const float ex = gamePos[0] - pw[0], ey = gamePos[1] - pw[1], ez = gamePos[2] - pw[2];
                const float eye = std::sqrt(ex*ex + ey*ey + ez*ez);
                Log("swing/arc:   pawn %.1f/%.1f/%.1f -> EYE OFFSET %.1f/%.1f/%.1f = %.1f uu. "
                    "First-person eye height is about 60-70 uu on an 87.5 uu cylinder. %s",
                    pw[0], pw[1], pw[2], ex, ey, ez, eye,
                    eye > 150.0f
                      ? "THIS IS THE LEVER: the camera is offset far further from the pawn than "
                        "an eye should be, so head pitch swings it through the measured arc. The "
                        "fix is whatever is inflating this offset."
                      : "This offset is normal, so the arc is NOT a long lever and the POV is "
                        "being driven some other way - do not chase the offset.");
            } else {
                Log("swing/arc:   pawn location unavailable (pawn=%p found=%d), so the eye "
                    "offset - the one number that would name the lever - is NOT measured here",
                    (void*)g_pePawn, (int)g_actorLocFound);
            }
        }
        else
            DVR_LOG_EVERY_MS(dvr::log::Cat::head, dvr::log::Level::Info, 30000,
                "swing/arc: eye moved %.1f uu over %.1f deg of pitch -> radius %.0f uu/rad "
                "(under the 60 uu/rad lead threshold; this is normal head motion)",
                posSpan, pitchSpanDeg, radiusUu);
    }
    winStart = now; n = 0;
    pMin = 1e30f; pMax = -1e30f;
    xMin = yMin = zMin = 1e30f; xMax = yMax = zMax = -1e30f;
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
        Log("swing: %s (seam)", b ? "ON" : "off");
        return true;
    }
    Log("swing: on|off (now %s). %d of %d dumps used. Read-only.",
        g_swOn ? "ON" : "off", g_swDumps, kSwMaxDumps);
    return true;
}
