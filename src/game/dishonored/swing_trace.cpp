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
