// game/dishonored/camera.cpp - see camera.h.
#define DVR_CAT ::dvr::log::Cat::head
#include "game/dishonored/camera.h"

#include "core/framework/status.h"
#include "core/util/log.h"
#include "core/util/mem.h"
#include "game/dishonored/patterns.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::camera {
namespace {

// sign: +1 the field holds the camera position, -1 it holds the NEGATED position
// (a view-matrix translation). Measured 2026-09-02 (ENGINE_NOTES, the per-eye
// camera seam): 0x330/0x350/0x374 read exactly -c5; 0x80/0x90/0xc4 hold a fixed
// offset vector, not the position, and keep +1 as a plain write.
struct Field { const char* name; uint32_t off; float sign; };
// The candidates: the matrix translation row, the two cached POV locations
// and the three POV rotator/location blocks the 38.24 eye clamp writes Z into.
const Field kFields[] = {
    {"0x80", kCamLoc0, 1.0f}, {"0x90", kCamLoc1, 1.0f}, {"0xc4", kCamLoc2, 1.0f},
    {"0x330", kPovOffs[0], -1.0f}, {"0x350", kPovOffs[1], -1.0f}, {"0x374", kPovOffs[2], -1.0f},
};
constexpr int kFieldCount = sizeof(kFields) / sizeof(kFields[0]);
constexpr int kEyetestFrames = 120;
constexpr int kEyetestBaselineFrames = 45;   // presents of c5 with NO write, per candidate

int   g_eye = 0;
float g_ipdM = 0.0f;
float g_scale = 100.0f;
int   g_field = -1;
float g_fovDeg = 0.0f;
float g_renderedFov = 0.0f;
float g_c5[3] = {0, 0, 0};
bool  g_c5Ok = false;

// The writer's memory of its last write, so a field the engine does NOT
// recompute is re-based instead of accumulated, and restored on release.
struct Writer {
    bool  lastOk = false;
    float last[3] = {0, 0, 0};      // the value we wrote
    float lastOff[3] = {0, 0, 0};   // the offset inside it
    uint32_t writes = 0;
};
Writer g_eyeWriter;

// Read the camera's right row (kCamRight: basis Y) as a unit vector.
bool read_right(uint8_t* cam, float r[3]) {
    if (!cam || !RangeReadable(cam + kCamRight, 12)) return false;
    const float* p = (const float*)(cam + kCamRight);
    const float n = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    if (!(n > 0.5f && n < 2.0f)) return false;   // not a basis row
    r[0] = p[0] / n; r[1] = p[1] / n; r[2] = p[2] / n;
    return true;
}

// Write base + off into cam+off, where base is the field's current value
// unless the current value is still OUR last write (a persistent field).
bool write_offset(uint8_t* cam, uint32_t fieldOff, const float off[3], Writer& w,
                  float outBase[3]) {
    if (!cam || !RangeReadable(cam + fieldOff, 12)) return false;
    float* v = (float*)(cam + fieldOff);
    float base[3];
    const bool persisted = w.lastOk && fabsf(v[0] - w.last[0]) < 0.01f &&
                           fabsf(v[1] - w.last[1]) < 0.01f && fabsf(v[2] - w.last[2]) < 0.01f;
    for (int i = 0; i < 3; ++i) base[i] = persisted ? (w.last[i] - w.lastOff[i]) : v[i];
    for (int i = 0; i < 3; ++i) {
        if (!(fabsf(base[i]) < 1.0e6f)) return false;   // not a location
        w.last[i] = base[i] + off[i];
        w.lastOff[i] = off[i];
        v[i] = w.last[i];
    }
    w.lastOk = true;
    ++w.writes;
    if (outBase) memcpy(outBase, base, sizeof(base));
    return true;
}

// Put a persistent field back to its base (no-op for a recomputed one).
void restore(uint8_t* cam, uint32_t fieldOff, Writer& w) {
    if (!w.lastOk) return;
    w.lastOk = false;
    if (!cam || !RangeReadable(cam + fieldOff, 12)) return;
    float* v = (float*)(cam + fieldOff);
    if (fabsf(v[0] - w.last[0]) < 0.01f && fabsf(v[1] - w.last[1]) < 0.01f &&
        fabsf(v[2] - w.last[2]) < 0.01f)
        for (int i = 0; i < 3; ++i) v[i] = w.last[i] - w.lastOff[i];
}

int field_index(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < kFieldCount; ++i)
        if (!_stricmp(kFields[i].name, name)) return i;
    return -1;
}

// ---- the eyetest ------------------------------------------------------------------
struct Eyetest {
    bool  active = false;
    float uu = 0.0f;
    int   only = -1;        // -1 = every candidate
    int   idx = 0;          // current candidate
    int   frames = 0;       // presents judged for the current candidate
    int   honoured = 0, discarded = 0, other = 0, noWrite = 0;
    double movedSum = 0.0;
    int   movedN = 0;
    bool  wrote = false;    // the script lane wrote this candidate at least once
    bool  writing = false;  // baseline phase over: the script lane writes
    int   baseFrames = 0;   // presents that fed the baseline
    int   baseWait = 0;     // presents spent waiting for a c5 in the baseline phase
    double baseSum[3] = {0, 0, 0};
    float baseline[3] = {0, 0, 0};
    bool  fieldSaid = false;
    float fieldVal[3] = {0, 0, 0};
    uint32_t ticks = 0, noCam = 0, noRight = 0, noField = 0;   // why a tick did not write
    float base[3] = {0, 0, 0}, right[3] = {0, 0, 0};
    bool  restorePending = false;
    Writer w;
    char  verdict[kFieldCount][16] = {};
} g_et;

void eyetest_next_candidate() {
    g_et.frames = g_et.honoured = g_et.discarded = g_et.other = g_et.noWrite = 0;
    g_et.ticks = g_et.noCam = g_et.noRight = g_et.noField = 0;
    g_et.writing = false;
    g_et.baseFrames = 0;
    g_et.baseWait = 0;
    g_et.baseSum[0] = g_et.baseSum[1] = g_et.baseSum[2] = 0.0;
    g_et.fieldSaid = false;
    g_et.movedSum = 0.0; g_et.movedN = 0;
    g_et.wrote = false;
    g_et.w = Writer();
}

void eyetest_verdict() {
    const Field& f = kFields[g_et.idx];
    const float mean = g_et.movedN ? (float)(g_et.movedSum / g_et.movedN) : 0.0f;
    const float want = g_et.uu * f.sign;   // what an honoured write moves c5 by
    const int judged = g_et.honoured + g_et.discarded + g_et.other;
    const char* verdict = "INCONCLUSIVE";
    if (!g_et.wrote) verdict = "NOT WRITTEN";
    else if (judged > 0 && g_et.honoured >= (judged * 4) / 5) verdict = "HONOURED";
    else if (judged > 0 && g_et.discarded >= (judged * 4) / 5) verdict = "DISCARDED";
    strncpy(g_et.verdict[g_et.idx], verdict, sizeof(g_et.verdict[0]) - 1);
    if (!g_et.wrote)
        DVR_WARN("camera/eyetest: %s asked %+.1f uu along right -> NOT WRITTEN in %d presents: "
                 "%u script ticks, %u with no live camera object, %u with no unit right row at "
                 "+0x60, %u with the field unreadable or not a location%s",
                 f.name, g_et.uu, g_et.frames, g_et.ticks, g_et.noCam, g_et.noRight, g_et.noField,
                 g_et.ticks == 0 ? " - the ProcessEvent hook is not running (gameplay?)" : "");
    else
        DVR_INFO("camera/eyetest: %s asked %+.1f uu along right -> c5 moved %+.1f uu (mean of %d): "
                 "%s %d/%d frames (discarded %d, other %d, no c5 %d)%s",
                 f.name, g_et.uu, mean, g_et.movedN, verdict,
                 !strcmp(verdict, "HONOURED") ? g_et.honoured : g_et.discarded, judged,
                 g_et.discarded, g_et.other, g_et.noWrite,
                 !strcmp(verdict, "HONOURED")
                     ? (f.sign < 0.0f ? " - the renderer drew from the offset position (the field "
                                        "holds -position, so the write is negated): this is the "
                                        "write point ([Camera] EyeField=)"
                                      : " - the renderer drew from the offset position: this is "
                                        "the write point ([Camera] EyeField=)")
                 : !strcmp(verdict, "DISCARDED") ? " - recomputed before the draw" : "");
    (void)want;
}

void eyetest_finish() {
    g_et.active = false;
    char line[256] = "";
    for (int i = 0; i < kFieldCount; ++i) {
        if (g_et.only >= 0 && i != g_et.only) continue;
        char one[48];
        _snprintf(one, sizeof(one), " %s=%s", kFields[i].name, g_et.verdict[i][0] ? g_et.verdict[i] : "-");
        one[sizeof(one) - 1] = 0;
        strncat(line, one, sizeof(line) - strlen(line) - 1);
    }
    int honouredIdx = -1;
    for (int i = 0; i < kFieldCount; ++i)
        if (!strcmp(g_et.verdict[i], "HONOURED")) { honouredIdx = i; break; }
    DVR_INFO("camera/eyetest: DONE (%+.1f uu):%s", g_et.uu, line);
    if (honouredIdx >= 0)
        DVR_INFO("camera/eyetest: set [Camera] EyeField=%s (or `camera eyefield %s`) - the "
                 "per-eye offset writes there from now on; record the verdicts in ENGINE_NOTES",
                 kFields[honouredIdx].name, kFields[honouredIdx].name);
    else
        DVR_WARN("camera/eyetest: NO candidate was honoured - the lateral eye offset needs a "
                 "later write point (ENGINE_NOTES, the per-eye camera seam: the position-only "
                 "matrix patch or the c0 translation). Record this in ENGINE_NOTES.");
}

} // namespace

// ---- eye selection and geometry -------------------------------------------------------
void set_eye(int sign) { g_eye = sign < 0 ? -1 : sign > 0 ? 1 : 0; }
int  eye() { return g_eye; }
void  set_ipd_m(float m) { if (m > 0.03f && m < 0.09f) g_ipdM = m; }
float ipd_m() { return g_ipdM; }
void  set_world_scale(float uuPerM) { if (uuPerM >= 1.0f && uuPerM <= 400.0f) g_scale = uuPerM; }
float world_scale() { return g_scale; }
float eye_offset_uu() { return (float)g_eye * 0.5f * g_ipdM * g_scale; }

bool set_eye_field(const char* name) {
    if (!name || !name[0] || !_stricmp(name, "none")) {
        if (g_field >= 0) DVR_INFO("camera: eye field cleared (was %s)", kFields[g_field].name);
        g_field = -1;
        return true;
    }
    const int i = field_index(name);
    if (i < 0) {
        DVR_WARN("camera: unknown eye field '%s' (0x80|0x90|0xc4|0x330|0x350|0x374|none) - "
                 "keeping %s", name, eye_field());
        return false;
    }
    g_field = i;
    g_eyeWriter = Writer();
    DVR_INFO("camera: eye field %s - the per-eye offset writes into camera+%s", kFields[i].name,
             kFields[i].name);
    return true;
}
const char* eye_field() { return g_field >= 0 ? kFields[g_field].name : "none"; }

// ---- fov --------------------------------------------------------------------------------
void  set_fov_deg(float deg) { g_fovDeg = deg; }
float fov_deg() { return g_fovDeg; }
void  note_rendered_fov(float deg) { g_renderedFov = deg; }
float rendered_fov_deg() { return g_renderedFov; }

// ---- render truth -----------------------------------------------------------------------
void note_render_pos(const float pos[3]) {
    if (!pos) return;
    g_c5[0] = pos[0]; g_c5[1] = pos[1]; g_c5[2] = pos[2];
    g_c5Ok = true;
}
bool render_pos(float out[3]) {
    if (!g_c5Ok || !out) return false;
    out[0] = g_c5[0]; out[1] = g_c5[1]; out[2] = g_c5[2];
    return true;
}

// ---- the writer (script lane) -----------------------------------------------------------
bool apply_eye_offset(uint8_t* camObj) {
    if (g_et.active) return false;   // the instrument owns the fields while it runs
    if (g_eye == 0) {
        if (g_eyeWriter.lastOk && g_field >= 0) restore(camObj, kFields[g_field].off, g_eyeWriter);
        return false;
    }
    if (g_field < 0) {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
                     "camera: a stereo method asked for eye %+d but no eye field is measured - "
                     "run `camera eyetest 100` in gameplay and set [Camera] EyeField= (the "
                     "render stays mono-positioned)", g_eye);
        return false;
    }
    if (!camObj) return false;
    float r[3];
    if (!read_right(camObj, r)) return false;
    const float uu = eye_offset_uu() * kFields[g_field].sign;
    const float off[3] = {r[0] * uu, r[1] * uu, r[2] * uu};
    const bool ok = write_offset(camObj, kFields[g_field].off, off, g_eyeWriter, nullptr);
    // 41.1 THE EYE-SEPARATION INSTRUMENT. The offset is laid along what
    // read_right() believes is the camera basis's right row (cam+kCamRight, an
    // ASSUMED offset). If that row does not actually rotate with the view, the
    // separation is laid along a FIXED WORLD AXIS: correct at one facing,
    // shrinking to nothing at ninety degrees to it, and reversed beyond that -
    // which is exactly "the eyes are misaligned with one another" and it would
    // come and go as the player turns. The first-3 line below cannot answer
    // that; this one can. Watch `right=` while turning a full circle:
    //   it ROTATES  -> the basis row is real, look elsewhere for the flicker
    //   it is FIXED -> kCamRight is not the right row, and that is the bug
    if (ok) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
            "camera/eyesep: eye %+d right=(%+.3f %+.3f %+.3f) offset %+.2f uu "
            "(ipd %.4f m, %.0f uu/m, field %s) - TURN IN A CIRCLE: if right= does "
            "not rotate, the separation is on a fixed world axis and that is the bug",
            g_eye, r[0], r[1], r[2], uu, g_ipdM, g_scale, kFields[g_field].name);
    }
    if (ok)
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 3,
                        "camera: eye %+d offset %+.2f uu along right (%.3f %.3f %.3f) -> camera+%s "
                        "(ipd %.4f m, %.0f uu/m)",
                        g_eye, uu, r[0], r[1], r[2], kFields[g_field].name, g_ipdM, g_scale);
    return ok;
}

// ---- the eyetest ------------------------------------------------------------------------
bool eyetest_start(float uu, const char* field) {
    if (g_et.active) { DVR_WARN("camera/eyetest: already running (candidate %s)", kFields[g_et.idx].name); return false; }
    if (!(uu >= 1.0f && uu <= 500.0f)) {
        DVR_WARN("camera/eyetest: <uu> must be 1..500 (got %.1f); 100 = 1 m at 100 uu/m", uu);
        return false;
    }
    int only = -1;
    if (field && field[0] && _stricmp(field, "all") != 0) {
        only = field_index(field);
        if (only < 0) {
            DVR_WARN("camera/eyetest: unknown field '%s' (0x80|0x90|0xc4|0x330|0x350|0x374|all)", field);
            return false;
        }
    }
    g_et = Eyetest();
    g_et.active = true;
    g_et.uu = uu;
    g_et.only = only;
    g_et.idx = only >= 0 ? only : 0;
    eyetest_next_candidate();
    DVR_INFO("camera/eyetest: START %+.1f uu along the camera's right row: per candidate %d "
             "presents of c5 baseline, then %d presents writing (%s). HONOURED = c5 (the draw's "
             "camera position) moves off its baseline by the asked amount; DISCARDED = it stays "
             "(the engine recomputed the field before the draw). Stand still in gameplay.",
             uu, kEyetestBaselineFrames, kEyetestFrames, only >= 0 ? kFields[only].name : "all six candidates");
    return true;
}

void eyetest_stop(const char* why) {
    if (!g_et.active) return;
    g_et.active = false;
    g_et.restorePending = g_et.w.lastOk;
    DVR_INFO("camera/eyetest: stopped (%s) at candidate %s", why ? why : "?", kFields[g_et.idx].name);
}

void eyetest_script_tick(uint8_t* camObj) {
    if (g_et.restorePending) {
        restore(camObj, kFields[g_et.idx].off, g_et.w);
        g_et.restorePending = false;
    }
    if (!g_et.active) return;
    ++g_et.ticks;
    if (!camObj) { ++g_et.noCam; return; }
    float r[3];
    if (!read_right(camObj, r)) { ++g_et.noRight; return; }
    memcpy(g_et.right, r, sizeof(r));
    const uint32_t fo = kFields[g_et.idx].off;
    if (!g_et.fieldSaid && RangeReadable(camObj + fo, 12)) {
        // The raw field next to the draw's c5, once per candidate: the frame
        // each lives in is a finding for ENGINE_NOTES whatever the verdict.
        const float* v = (const float*)(camObj + fo);
        memcpy(g_et.fieldVal, v, sizeof(g_et.fieldVal));
        g_et.fieldSaid = true;
        DVR_INFO("camera/eyetest: %s reads (%.1f %.1f %.1f); c5 (%.1f %.1f %.1f); right (%.3f %.3f %.3f)",
                 kFields[g_et.idx].name, v[0], v[1], v[2], g_c5[0], g_c5[1], g_c5[2], r[0], r[1], r[2]);
    }
    if (!g_et.writing) return;   // baseline phase: look, do not touch
    const float su = g_et.uu * kFields[g_et.idx].sign;   // the field's own form
    const float off[3] = {r[0] * su, r[1] * su, r[2] * su};
    float base[3];
    if (!write_offset(camObj, fo, off, g_et.w, base)) { ++g_et.noField; return; }
    memcpy(g_et.base, base, sizeof(base));
    g_et.wrote = true;
}

void eyetest_present_tick() {
    if (!g_et.active) return;
    if (!g_et.writing) {
        // Baseline: c5 while nothing is written. The player stands still, so
        // this is the position the honoured case must move away from.
        if (g_c5Ok) {
            for (int i = 0; i < 3; ++i) g_et.baseSum[i] += g_c5[i];
            ++g_et.baseFrames;
        }
        if (g_et.baseFrames >= kEyetestBaselineFrames) {
            for (int i = 0; i < 3; ++i) g_et.baseline[i] = (float)(g_et.baseSum[i] / g_et.baseFrames);
            g_et.writing = true;
        } else if (++g_et.baseWait >= kEyetestFrames * 3) {
            // c5 never arrived: no draw is passing through the constant hook.
            DVR_WARN("camera/eyetest: %s - no c5 readback in %d presents (the game is not "
                     "drawing a scene, or the constant hook is not seeing c5); stopping",
                     kFields[g_et.idx].name, g_et.baseWait);
            eyetest_stop("no c5");
        }
        return;
    }
    ++g_et.frames;
    if (!g_et.wrote) {
        ++g_et.noWrite;
    } else if (!g_c5Ok) {
        ++g_et.noWrite;
    } else {
        const float d[3] = {g_c5[0] - g_et.baseline[0], g_c5[1] - g_et.baseline[1],
                            g_c5[2] - g_et.baseline[2]};
        const float moved = d[0] * g_et.right[0] + d[1] * g_et.right[1] + d[2] * g_et.right[2];
        g_et.movedSum += moved; ++g_et.movedN;
        const float band = 0.25f * g_et.uu;
        if (fabsf(moved - g_et.uu) <= band) ++g_et.honoured;
        else if (fabsf(moved) <= band) ++g_et.discarded;
        else ++g_et.other;
    }
    if (g_et.frames < kEyetestFrames) return;
    eyetest_verdict();
    g_et.restorePending = g_et.w.lastOk;
    if (g_et.only >= 0 || g_et.idx + 1 >= kFieldCount) { eyetest_finish(); return; }
    ++g_et.idx;
    eyetest_next_candidate();
}

bool eyetest_active() { return g_et.active; }

// ---- status -----------------------------------------------------------------------------
void status(dvr::status::Writer& w) {
    w.kv("eye", g_eye);
    w.kv("ipdM", (double)g_ipdM);
    w.kv("uuPerM", (double)g_scale);
    w.kv("eyeOffsetUu", (double)eye_offset_uu());
    w.kv("eyeField", eye_field());
    w.kv("eyeWrites", (unsigned long)g_eyeWriter.writes);
    w.kv("fovDeg", (double)g_fovDeg);
    w.kv("renderedFovDeg", (double)g_renderedFov);
    w.kv("c5ok", g_c5Ok);
    w.obj("eyetest");
    w.kv("active", g_et.active);
    w.kv("candidate", g_et.active ? kFields[g_et.idx].name : "-");
    w.kv("frames", g_et.frames);
    w.kv("honoured", g_et.honoured);
    w.kv("discarded", g_et.discarded);
    w.end_obj();
}

void log_status() {
    DVR_INFO("camera: eye=%+d ipd=%.4f m scale=%.0f uu/m offset=%+.2f uu field=%s writes=%lu | "
             "fov lever=%.0f rendered=%.1f | c5=%s(%.1f %.1f %.1f) | eyetest=%s",
             g_eye, g_ipdM, g_scale, eye_offset_uu(), eye_field(), (unsigned long)g_eyeWriter.writes,
             g_fovDeg, g_renderedFov, g_c5Ok ? "" : "none ", g_c5[0], g_c5[1], g_c5[2],
             g_et.active ? kFields[g_et.idx].name : "idle");
}

} // namespace dvr::camera
