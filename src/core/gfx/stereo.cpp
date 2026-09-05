// core/gfx/stereo.cpp - the stereo-strategy registry and the frame path's
// entry points (see stereo.h). Present thread only.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/stereo.h"

#include "core/framework/status.h"
#include "core/gfx/capture.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace dvr::stereo {
namespace {

constexpr int kMaxMethods = 4;
IStereo*     g_methods[kMaxMethods] = {};
int          g_methodCount = 0;
IStereo*     g_active = nullptr;
bool         g_registered = false;
FrameOutput  g_last;
uint32_t     g_framesOut = 0;
OverlayDrawFn g_overlay = nullptr;

// The beat: one line every 3 s naming the method and what it produced, so a
// log can say which eyes flowed and at what rate. The counters here are
// populations, not evidence on their own: a mono method produces 0 left and 0
// right BY DESIGN, and the line says so.
uint64_t g_beatMs = 0;
uint32_t g_beatOut = 0, g_beatL = 0, g_beatR = 0, g_beatMono = 0, g_beatNone = 0;
int      g_projOverride = -1;   // -1 auto, 0 off, 1 on
int      g_holdUntagged = 3;    // 41.1: max CONSECUTIVE untagged presents to suppress (0 = off; 3 ships, headset-judged)
uint32_t g_holdsDone = 0;       // how many this session
char     g_configMethod[16] = "";   // [Stereo] Method, applied once the game side is up
// The SELECTION and whether it RUNS are two things (41.1, the F10 tickbox):
// `g_wanted` is the method the player chose ([Stereo] Method, `stereo <name>`),
// `g_armed` whether it is live. Parking (armed off) selects the mono screen
// without forgetting the choice, so a SAVE AS DEFAULTS while parked keeps
// Method=reentry Armed=0 instead of losing the method for good.
char     g_wanted[16] = "mono";
bool     g_armed = true;

// The runtime's [pair] probe, drained ONCE per beat here (its maxima reset on
// read, so a second drainer would steal the window); status.json and the
// `stereo status` word print this cached snapshot.
dvr::vr::PairProbe g_pairBeat;
uint64_t           g_pairBeatMs = 0;
dvr::vr::PairProbe g_pairPrev;     // the previous beat's cumulative counters (for the deltas)

} // namespace

void register_method(IStereo* m) {
    if (!m || g_methodCount >= kMaxMethods) return;
    for (int i = 0; i < g_methodCount; ++i)
        if (!_stricmp(g_methods[i]->name(), m->name())) return;
    g_methods[g_methodCount++] = m;
    if (!g_active && m->implemented()) g_active = m;
    DVR_INFO("stereo: method '%s' registered%s%s", m->name(),
             m->implemented() ? "" : " (design stub - not implemented)",
             g_active == m ? " - default" : "");
}

void register_all() {
    if (g_registered) return;
    g_registered = true;
    register_method(create_mono_screen());
    register_method(create_aer());
    register_method(create_reentry());
}

bool select(const char* name) {
    if (!name || !name[0]) {
        DVR_WARN("stereo: select needs a name (mono|aer|reentry); staying on '%s'", active_name());
        return false;
    }
    IStereo* found = nullptr;
    for (int i = 0; i < g_methodCount; ++i)
        if (!_stricmp(g_methods[i]->name(), name)) found = g_methods[i];
    if (!found) {
        char list[64] = "";
        for (int i = 0; i < g_methodCount; ++i) {
            strncat(list, i ? " " : "", sizeof(list) - strlen(list) - 1);
            strncat(list, g_methods[i]->name(), sizeof(list) - strlen(list) - 1);
        }
        DVR_WARN("stereo: no method named '%s' (registered: %s); staying on '%s'", name,
                 g_methodCount ? list : "none", active_name());
        return false;
    }
    if (!found->implemented()) {
        DVR_WARN("stereo: '%s' is a design stub, not implemented - staying on '%s'. %s",
                 found->name(), active_name(), found->note());
        return false;
    }
    if (found == g_active) {
        DVR_DEBUG("stereo: '%s' is already the active method", found->name());
        return true;
    }
    IStereo* prev = g_active;
    if (prev) prev->shutdown();
    g_active = found;
    g_last = FrameOutput{};
    DVR_INFO("stereo: method %s -> %s (live; the next present uses it)",
             prev ? prev->name() : "none", found->name());
    return true;
}

void set_config_method(const char* name) {
    strncpy(g_configMethod, name ? name : "", sizeof(g_configMethod) - 1);
    g_configMethod[sizeof(g_configMethod) - 1] = 0;
}

void apply_config_method() {
    static bool applied = false;
    if (!g_configMethod[0] || applied) return;   // the game calls Direct3DCreate9 twice
    applied = true;
    const bool ok = choose(g_configMethod);
    if (!ok && !g_active) select("mono");
    DVR_INFO("stereo: [Stereo] Method=%s Armed=%d applied after the game side registered -> active '%s'%s",
             g_configMethod, g_armed ? 1 : 0, active_name(),
             !g_armed ? " (PARKED on the mono screen: the F10 tickbox or `stereo arm on` runs it)"
             : ok ? "" : " (refused above; the mono screen runs)");
}

bool choose(const char* name) {
    if (!name || !name[0]) return false;
    strncpy(g_wanted, name, sizeof(g_wanted) - 1);
    g_wanted[sizeof(g_wanted) - 1] = 0;
    if (!g_armed) {
        DVR_INFO("stereo: '%s' selected while parked - it runs when armed (the F10 tickbox, `stereo arm on`)", g_wanted);
        return true;
    }
    return select(name);
}

void set_armed(bool on) {
    if (on == g_armed) return;
    g_armed = on;
    if (!on) {
        if (g_active && _stricmp(g_active->name(), "mono") != 0) select("mono");
        DVR_INFO("stereo: armed -> off (selected '%s', active '%s') - parked on the mono screen; the selection is "
                 "kept, `stereo arm on` or the F10 tickbox re-arms it", g_wanted, active_name());
    } else {
        const bool ok = select(g_wanted);
        DVR_INFO("stereo: armed -> ON (selected '%s', active '%s')%s", g_wanted, active_name(),
                 ok ? " - the next present uses it" : " (the selection refused above; the mono screen runs)");
    }
}

bool armed() { return g_armed; }
const char* wanted_name() { return g_wanted; }

IStereo* active() { return g_active; }
const char* active_name() { return g_active ? g_active->name() : "none"; }

void begin_frame(const FrameInput& in) {
    if (g_active) g_active->begin_frame(in);
}

bool end_frame(const FrameDevices& d, FrameOutput& out) {
    out = FrameOutput{};
    bool ok = g_active && g_active->end_frame(d, out);
    if (!ok) out = FrameOutput{};
    g_last = out;
    if (out.tex) {
        ++g_framesOut;
        ++g_beatOut;
        if (out.eyeSign < 0) ++g_beatL;
        else if (out.eyeSign > 0) ++g_beatR;
        else ++g_beatMono;
    } else {
        ++g_beatNone;
    }
    const uint64_t now = GetTickCount64();
    if (g_beatMs == 0) g_beatMs = now;
    else if (now - g_beatMs >= 3000) {
        const double s = (double)(now - g_beatMs) / 1000.0;
        DVR_INFO("stereo: beat method=%s out/s=%.0f L/s=%.0f R/s=%.0f mono/s=%.0f none/s=%.0f %ux%u"
                 "%s%s%s",
                 active_name(), g_beatOut / s, g_beatL / s, g_beatR / s, g_beatMono / s,
                 g_beatNone / s, out.w, out.h,
                 (g_beatL == 0 && g_beatR == 0 && g_beatMono > 0)
                     ? " (L/R read 0 by design on the mono screen)" : "",
                 !g_armed ? " (PARKED: the selection is kept, not armed)" : "",
                 dvr::capture::mode() == dvr::capture::Mode::Off
                     ? " capture OFF by request (frozen image expected)" : "");
        // The eyes line: per-eye image age in PRESENTS at the last stereo submit
        // and the pairing counters as window deltas. A stereo submit shows each
        // eye swapchain's last released image, so an eye older than one present
        // is a previous tick shown again - the instrument that can print the
        // unwelcome answer for "one eye stopped taking fresh frames".
        g_pairPrev = g_pairBeat;
        dvr::vr::pair_probe(&g_pairBeat);
        g_pairBeatMs = now;
        if (wants_projection()) {
            const dvr::vr::PairProbe& p = g_pairBeat;
            const dvr::vr::PairProbe& q = g_pairPrev;
            const uint32_t submits = p.stereoSubmits - q.stereoSubmits;
            DVR_INFO("stereo: eyes ageL=%u ageR=%u presents at the last stereo submit (max this window L=%u R=%u; "
                     "healthy 1/0) | stereoSubmits=%u pairs=%u aborts=%u (left=%u untagged=%u expired=%u) "
                     "staleEye L=%u R=%u eaten=%u | window %.0f s%s",
                     p.agePresL, p.agePresR, p.agePresMaxL, p.agePresMaxR, submits, p.pairs - q.pairs,
                     p.aborts - q.aborts, p.abortLeft - q.abortLeft, p.abortUntagged - q.abortUntagged,
                     p.abortExpired - q.abortExpired, p.stalePresL - q.stalePresL, p.stalePresR - q.stalePresR,
                     p.eatenNoFrame - q.eatenNoFrame, s,
                     submits == 0 ? " (stereoSubmits 0 this window: the quad screen or an untagged stream - "
                                    "the ages read 0 by design)" : "");
        }
        // The RATE line (41.1, session 13): the headset's own cadence next to
        // ours, on one line, because "the game outruns the headset and the
        // runtime absorbs it by blocking at xrEndFrame" was an inference from a
        // phase name and this project has paid for inferences that read well.
        //   slots/s  the headset, from the runtime's predictedDisplayPeriod
        //   submits/s  xrEndFrame from the present path - ONE PER STEREO PAIR,
        //              so it is the tick rate and NOT out/s (which counts
        //              presents, two per tick under reentry)
        //   endFrame   what one submit costs: the mean is the pacing wait if
        //              there is one, the max is the hitch a player feels
        // What would make it move, and what each answer means: more ticks raise
        // submits/s (over a slot rate that does not move, the runtime has to
        // block somewhere, and the throttle reading is confirmed); a headset set
        // to another refresh moves slots/s; a runtime that leaves the period at 0
        // prints UNKNOWN and gets no verdict at all. Submits BELOW the slot rate
        // is the unwelcome answer and it falsifies the throttle reading outright.
        // The verdict needs branches, so it cannot live inside the DVR_INFO
        // arguments: the whole block takes the category gate itself.
        if (::dvr::log::enabled(DVR_CAT, ::dvr::log::Level::Info)) {
            const dvr::vr::PairProbe& p = g_pairBeat;
            const dvr::vr::PairProbe& q = g_pairPrev;
            const double periodMs = (double)p.displayPeriodNs / 1.0e6;
            const double slots = periodMs > 0.0 ? 1000.0 / periodMs : 0.0;
            const uint32_t subs = p.endFrames - q.endFrames;
            const double subsPerS = subs / s;
            const double efMean = subs ? (double)(p.endFrameSumUs - q.endFrameSumUs) / 1000.0 / subs : 0.0;
            const double ratio = slots > 0.0 ? subsPerS / slots : 0.0;
            char hmd[64], verdict[320];
            if (periodMs > 0.0) _snprintf(hmd, sizeof(hmd), "%.2f ms (%.1f Hz)", periodMs, slots);
            else strcpy_s(hmd, sizeof(hmd), "UNKNOWN (the runtime left predictedDisplayPeriod at 0)");
            if (periodMs <= 0.0)
                strcpy_s(verdict, sizeof(verdict),
                         "no slot comparison is possible on this runtime - the submit-throttle reading stays "
                         "an inference, and only the endFrame cost above is evidence");
            else if (subs == 0)
                strcpy_s(verdict, sizeof(verdict),
                         "NO SUBMITS this window (the session is not submitting: parked, unfocused, or the "
                         "pace guard is skipping) - the rates on this line read 0 BY DESIGN, not by fault");
            else if (ratio > 1.05)
                _snprintf(verdict, sizeof(verdict),
                          "OVER-SUBMITTING %.2fx: more submits than the headset has slots, so the runtime MUST "
                          "block somewhere to absorb the surplus - the submit-throttle reading is confirmed and "
                          "the lever is to stop producing frames nobody sees", ratio);
            else if (ratio >= 0.95)
                _snprintf(verdict, sizeof(verdict),
                          "MATCHED %.2fx: one submit per display slot - the endFrame MEAN is the pacing wait and "
                          "is not a hitch; only the max is", ratio);
            else
                _snprintf(verdict, sizeof(verdict),
                          "UNDER-SUBMITTING %.2fx: display slots are going UNFILLED, so xrEndFrame is not "
                          "throttling a surplus - the present-tail stalls are genuine hitches and their cause is "
                          "upstream of the headset's cadence", ratio);
            DVR_INFO("stereo: rate hmd=%s slots/s=%.1f | presents/s=%.0f submits/s=%.0f (one xrEndFrame per pair) "
                     "| endFrame mean=%.2f ms max=%.1f ms over %u submits | %s",
                     hmd, slots, (g_beatOut + g_beatNone) / s, subsPerS, efMean,
                     p.endFrameMaxUs / 1000.0, subs, verdict);
        }
        g_beatMs = now;
        g_beatOut = g_beatL = g_beatR = g_beatMono = g_beatNone = 0;
    }
    return ok;
}

void on_reset() {
    for (int i = 0; i < g_methodCount; ++i) g_methods[i]->on_reset();
    g_last = FrameOutput{};
}

void shutdown() {
    for (int i = 0; i < g_methodCount; ++i) g_methods[i]->shutdown();
    g_last = FrameOutput{};
}

const FrameOutput& last_output() { return g_last; }
uint32_t frames_out() { return g_framesOut; }

bool wants_projection() {
    if (g_projOverride >= 0) return g_projOverride == 1;
    return g_active && g_active->wants_projection();
}
void set_projection_override(int mode) {
    const int m = mode < 0 ? -1 : mode > 0 ? 1 : 0;
    if (m == g_projOverride) return;
    g_projOverride = m;
    DVR_INFO("stereo: projection %s (the frame path arms camera mode on the next present: %s)",
             projection_override_name(),
             wants_projection() ? "PROJECTION layer, per-eye poses, the FOV lever follows the frame aspect"
                                : "the head-locked quad screen, lever as configured");
}
const char* projection_override_name() {
    return g_projOverride < 0 ? "auto" : g_projOverride ? "on" : "off";
}

// 41.1 (Dishonored): the untagged hold. The method asks whether it may keep
// this untagged present off the wire; stereo.cpp owns the count so the beat
// and status can report it without the method exporting a global.
int hold_untagged() { return g_holdUntagged; }
void set_hold_untagged(int n) {
    const int v = n < 0 ? 0 : n > 30 ? 30 : n;
    if (v == g_holdUntagged) return;
    g_holdUntagged = v;
    DVR_INFO("stereo: hold %d - up to %d consecutive untagged present(s) are held back (the compositor keeps the "
             "previous pair) before the mono path is used; 0 = off, the untagged present goes out as mono. "
             "`stereo status` and the beat's holds= say how many were held.", v, v);
}
uint32_t holds_done() { return g_holdsDone; }
void note_hold() { ++g_holdsDone; }   // called by the method when it suppresses one

void status(dvr::status::Writer& w) {
    w.kv("method", active_name());
    w.kv("wanted", g_wanted);
    w.kv("armed", g_armed);
    w.kv("w", (int)g_last.w);
    w.kv("h", (int)g_last.h);
    w.kv("eyeSign", (int)g_last.eyeSign);
    w.kv("framesOut", (unsigned long)g_framesOut);
    w.kv("nextEye", g_active ? g_active->eye_for_next_frame() : 0);
    w.kv("projection", wants_projection());
    w.kv("projectionOverride", projection_override_name());
    w.kv("holdUntagged", g_holdUntagged);
    w.kv("holds", (unsigned long)g_holdsDone);
    w.kv("camMode", dvr::vr::vr_camera_mode());
    w.kv("cineActive", dvr::vr::cinematic_active());
    {   // the runtime's pair probe as the beat last drained it (cumulative counters)
        const dvr::vr::PairProbe& p = g_pairBeat;
        w.obj("pair");
        w.kv("beatAgeMs", (unsigned long)(g_pairBeatMs ? GetTickCount64() - g_pairBeatMs : 0));
        w.kv("ageL", (unsigned long)p.agePresL); w.kv("ageR", (unsigned long)p.agePresR);
        w.kv("ageMaxL", (unsigned long)p.agePresMaxL); w.kv("ageMaxR", (unsigned long)p.agePresMaxR);
        w.kv("staleL", (unsigned long)p.stalePresL); w.kv("staleR", (unsigned long)p.stalePresR);
        w.kv("staleMsL", (unsigned long)p.staleL); w.kv("staleMsR", (unsigned long)p.staleR);
        w.kv("stereoSubmits", (unsigned long)p.stereoSubmits);
        w.kv("pairs", (unsigned long)p.pairs); w.kv("aborts", (unsigned long)p.aborts);
        w.kv("abortLeft", (unsigned long)p.abortLeft); w.kv("abortUntagged", (unsigned long)p.abortUntagged);
        w.kv("abortExpired", (unsigned long)p.abortExpired);
        w.kv("capL", (unsigned long)p.cap[0]); w.kv("capR", (unsigned long)p.cap[1]);
        w.kv("acqFail", (unsigned long)p.acqFail); w.kv("waitFail", (unsigned long)p.waitFail);
        w.kv("untaggedProj", (unsigned long)p.untaggedProj);
        w.kv("ringPushed", (unsigned long)p.ringPushed); w.kv("ringPopped", (unsigned long)p.ringPopped);
        w.kv("phaseAvailable", p.phaseAvailable);
        w.kv("phaseMeanMs", p.phaseCount ? (double)p.phaseSumUs / 1000.0 / p.phaseCount : 0.0);
        w.kv("phaseLastMs", (double)p.phaseLastUs / 1000.0);
        w.kv("phaseMaxMs", (double)p.phaseMaxUs / 1000.0);
        w.kv("phaseMissedPct", p.phaseCount ? 100.0 * p.phaseMissed / p.phaseCount : 0.0);
        w.kv("phasePairs", (unsigned long)p.phaseCount);
        // Session 13: the headset's cadence and the submit rate. displayHz is 0
        // when the runtime does not fill predictedDisplayPeriod - unknown, not
        // "0 Hz". endFrameMaxMs is the LAST BEAT WINDOW's worst submit (the beat
        // drains it); the mean is over the whole session.
        w.kv("displayPeriodMs", (double)p.displayPeriodNs / 1.0e6);
        w.kv("displayHz", p.displayPeriodNs > 0 ? 1.0e9 / (double)p.displayPeriodNs : 0.0);
        w.kv("endFrames", (unsigned long)p.endFrames);
        w.kv("endFrameMeanMs", p.endFrames ? (double)p.endFrameSumUs / 1000.0 / p.endFrames : 0.0);
        w.kv("endFrameMaxMs", (double)p.endFrameMaxUs / 1000.0);
        w.end_obj();
    }
    if (g_active) g_active->status(w);
}

void log_status() {
    DVR_INFO("stereo: method=%s (selected %s, armed %d) framesOut=%lu last=%ux%u eyeSign=%d nextEye=%d projection=%s (%s) "
             "camMode=%d cineQuad=%d registered=%d",
             active_name(), g_wanted, g_armed ? 1 : 0, (unsigned long)g_framesOut, g_last.w, g_last.h, g_last.eyeSign,
             g_active ? g_active->eye_for_next_frame() : 0, wants_projection() ? "yes" : "no",
             projection_override_name(), (int)dvr::vr::vr_camera_mode(), (int)dvr::vr::cinematic_active(),
             g_methodCount);
    for (int i = 0; i < g_methodCount; ++i)
        DVR_INFO("stereo:   %s%s%s", g_methods[i]->name(),
                 g_methods[i] == g_active ? " (active)" : "",
                 g_methods[i]->implemented() ? "" : " (design stub)");
    {   // the pair probe as the beat last drained it
        const dvr::vr::PairProbe& p = g_pairBeat;
        DVR_INFO("stereo: pair (as of %lu ms ago): ages L=%u R=%u presents (healthy 1/0) | stereoSubmits=%lu pairs=%lu "
                 "aborts=%lu (left=%lu untagged=%lu expired=%lu) staleEye L=%lu R=%lu | caps L=%lu R=%lu acqFail=%lu "
                 "waitFail=%lu untaggedProj=%lu | ring pushed=%lu popped=%lu",
                 (unsigned long)(g_pairBeatMs ? GetTickCount64() - g_pairBeatMs : 0), p.agePresL, p.agePresR,
                 (unsigned long)p.stereoSubmits, (unsigned long)p.pairs, (unsigned long)p.aborts,
                 (unsigned long)p.abortLeft, (unsigned long)p.abortUntagged, (unsigned long)p.abortExpired,
                 (unsigned long)p.stalePresL, (unsigned long)p.stalePresR, (unsigned long)p.cap[0],
                 (unsigned long)p.cap[1], (unsigned long)p.acqFail, (unsigned long)p.waitFail,
                 (unsigned long)p.untaggedProj, (unsigned long)p.ringPushed, (unsigned long)p.ringPopped);
        if (p.phaseAvailable)
            DVR_INFO("stereo: pair phase %+.1f ms mean (last %+.1f, max %+.1f) over %lu pairs, missed slot %lu (%.0f%%) - "
                     "close minus predictedDisplayTime; negative = closed before its slot",
                     p.phaseCount ? (double)p.phaseSumUs / 1000.0 / p.phaseCount : 0.0, (double)p.phaseLastUs / 1000.0,
                     (double)p.phaseMaxUs / 1000.0, (unsigned long)p.phaseCount, (unsigned long)p.phaseMissed,
                     p.phaseCount ? 100.0 * p.phaseMissed / p.phaseCount : 0.0);
        else
            DVR_INFO("stereo: pair phase n/a (the runtime offers no XR_KHR_win32_convert_performance_counter_time)");
        // The rate, live rather than as of the last beat: the period can change
        // under us (the headset's refresh setting) and this word is what a
        // tester runs when they want an answer now.
        const int64_t periodNs = dvr::vr::display_period_ns();
        if (periodNs > 0)
            DVR_INFO("stereo: rate hmd=%.2f ms (%.1f Hz) slots/s=%.1f | submits=%lu since start, endFrame mean=%.2f ms "
                     "(max %.1f ms in the last beat window) - one xrEndFrame per pair, so submits track the TICK rate",
                     (double)periodNs / 1.0e6, 1.0e9 / (double)periodNs, 1.0e9 / (double)periodNs,
                     (unsigned long)p.endFrames, p.endFrames ? (double)p.endFrameSumUs / 1000.0 / p.endFrames : 0.0,
                     (double)p.endFrameMaxUs / 1000.0);
        else
            DVR_INFO("stereo: rate hmd=UNKNOWN - this runtime leaves xrWaitFrame's predictedDisplayPeriod at 0, so "
                     "there is no slot rate to compare against | submits=%lu since start, endFrame mean=%.2f ms",
                     (unsigned long)p.endFrames,
                     p.endFrames ? (double)p.endFrameSumUs / 1000.0 / p.endFrames : 0.0);
    }
}

void set_overlay_draw(OverlayDrawFn fn) { g_overlay = fn; }
OverlayDrawFn overlay_draw() { return g_overlay; }

} // namespace dvr::stereo
