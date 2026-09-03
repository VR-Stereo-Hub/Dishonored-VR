// game/dishonored/camera.cpp - see camera.h.
#define DVR_CAT ::dvr::log::Cat::head
#include "game/dishonored/camera.h"

#include "core/framework/status.h"
#include "core/gfx/capture.h"
#include "core/gfx/stereo.h"
#include "core/util/log.h"
#include "core/util/mem.h"
#include "game/dishonored/patterns.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::camera {
namespace {

// sign  : what a wanted WORLD displacement must be multiplied by to become the
//         delta added to the field. +1 = the field holds the camera position.
// c5Sign: how the c5 constant answers that displacement (+1 it follows, -1 it
//         is the negation).
//
// CORRECTED 2026-09-03 (ENGINE_NOTES, "The camera field holds the position,
// c5 is its negation"). Session 5 read 0x330 as a negated position because it
// reads exactly -c5 and assumed c5 was the camera position; the PICTURE says
// otherwise - writing +X into the field moves the rendered view by +X, and the
// shipped 38.24 crouch clamp agrees (it compares this field's Z against the
// pawn's world Z). So the field IS the position and c5 is its negation; the
// eyetest's old HONOURED verdict had the polarity backwards, which put every
// eye offset and every lean in the wrong direction in the headset.
struct Field { const char* name; uint32_t off; float sign; float c5Sign; };
// The candidates: the matrix translation row, the two cached POV locations
// and the three POV rotator/location blocks the 38.24 eye clamp writes Z into.
const Field kFields[] = {
    {"0x80", kCamLoc0, 1.0f, 1.0f}, {"0x90", kCamLoc1, 1.0f, 1.0f}, {"0xc4", kCamLoc2, 1.0f, 1.0f},
    {"0x330", kPovOffs[0], 1.0f, -1.0f}, {"0x350", kPovOffs[1], 1.0f, -1.0f},
    {"0x374", kPovOffs[2], 1.0f, -1.0f},
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

// ---- positional tracking on the seam ----------------------------------------
volatile float g_pos[3] = {0, 0, 0};   // right, up, forward (uu); present thread writes
// (the lane is resolved by pos_lane(): [PosTrack] Lane=auto follows the projection claim)
float    g_ceilZ = 0.0f;
bool     g_ceilOn = false;
bool     g_basisSaid = false;
uint32_t g_basisBad = 0;

// Read one basis row as a unit vector (a row of the camera's matrix at
// +0x50/+0x60/+0x70). False when the norm is not near 1: not a basis row.
bool read_row(uint8_t* cam, uint32_t off, float r[3]) {
    if (!cam || !RangeReadable(cam + off, 12)) return false;
    const float* p = (const float*)(cam + off);
    const float n = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    if (!(n > 0.5f && n < 2.0f)) return false;   // not a basis row
    r[0] = p[0] / n; r[1] = p[1] / n; r[2] = p[2] / n;
    return true;
}

// Read the camera's right row (kCamRight: basis Y) as a unit vector.
bool read_right(uint8_t* cam, float r[3]) { return read_row(cam, kCamRight, r); }

// The three rows, validated orthogonal (|dot| < 0.05 pairwise) and logged
// once either way: a position offset is written along a measured basis or
// not at all - never along a guessed axis.
bool read_basis(uint8_t* cam, float f[3], float r[3], float u[3]) {
    const bool ok = read_row(cam, kCamFwd, f) && read_row(cam, kCamRight, r) && read_row(cam, kCamUp, u);
    float dfr = 0, dfu = 0, dru = 0;
    if (ok) {
        dfr = f[0] * r[0] + f[1] * r[1] + f[2] * r[2];
        dfu = f[0] * u[0] + f[1] * u[1] + f[2] * u[2];
        dru = r[0] * u[0] + r[1] * u[1] + r[2] * u[2];
    }
    const bool ortho = ok && fabsf(dfr) < 0.05f && fabsf(dfu) < 0.05f && fabsf(dru) < 0.05f;
    if (!ortho) {
        ++g_basisBad;
        if (!g_basisSaid) {
            g_basisSaid = true;
            DVR_WARN("camera: basis rows at +0x%x/+0x%x/+0x%x are not an orthonormal basis (%s; dots "
                     "%.3f %.3f %.3f) - the position offset is NOT written (the eye offset still is, "
                     "along the right row alone)",
                     kCamFwd, kCamRight, kCamUp, ok ? "rows read" : "a row is not unit length", dfr, dfu, dru);
        }
        return false;
    }
    if (!g_basisSaid) {
        g_basisSaid = true;
        DVR_INFO("camera: basis rows fwd=(%.3f %.3f %.3f) right=(%.3f %.3f %.3f) up=(%.3f %.3f %.3f) "
                 "orthonormal (|dots| %.3f %.3f %.3f) - the position offset writes along them",
                 f[0], f[1], f[2], r[0], r[1], r[2], u[0], u[1], u[2], fabsf(dfr), fabsf(dfu), fabsf(dru));
    }
    return true;
}

// The field's base: its current value unless that is still OUR last write (a
// persistent field), in which case the base is what we wrote minus our offset.
bool current_base(uint8_t* cam, uint32_t fieldOff, const Writer& w, float base[3]) {
    if (!cam || !RangeReadable(cam + fieldOff, 12)) return false;
    const float* v = (const float*)(cam + fieldOff);
    const bool persisted = w.lastOk && fabsf(v[0] - w.last[0]) < 0.01f &&
                           fabsf(v[1] - w.last[1]) < 0.01f && fabsf(v[2] - w.last[2]) < 0.01f;
    for (int i = 0; i < 3; ++i) {
        base[i] = persisted ? (w.last[i] - w.lastOff[i]) : v[i];
        if (!(fabsf(base[i]) < 1.0e6f)) return false;   // not a location
    }
    return true;
}

// Write base + off into cam+off, where base is the field's current value
// unless the current value is still OUR last write (a persistent field).
bool write_offset(uint8_t* cam, uint32_t fieldOff, const float off[3], Writer& w,
                  float outBase[3]) {
    float base[3];
    if (!current_base(cam, fieldOff, w, base)) return false;
    float* v = (float*)(cam + fieldOff);
    for (int i = 0; i < 3; ++i) {
        w.last[i] = base[i] + off[i];
        w.lastOff[i] = off[i];
        v[i] = w.last[i];
    }
    w.lastOk = true;
    ++w.writes;
    if (outBase) memcpy(outBase, base, sizeof(base));
    return true;
}

// ---- the postest (positional instrument) -------------------------------------
constexpr int kPostestFrames = 120;
constexpr int kPostestBaselineFrames = 45;
struct Postest {
    bool    active = false;
    PosLane lane = PosLane::Vp;
    float   cmd[3] = {0, 0, 0};       // the commanded right/up/forward (uu)
    bool    writing = false;          // baseline over
    int     baseFrames = 0, baseWait = 0, frames = 0;
    double  baseSum[3] = {0, 0, 0};
    float   baseline[3] = {0, 0, 0};
    double  measSum[3] = {0, 0, 0};   // c5 travel projected on the basis (camera lane)
    int     measN = 0, noC5 = 0;
    float   f[3] = {1, 0, 0}, r[3] = {0, 1, 0}, u[3] = {0, 0, 1};   // the basis at the last write
    bool    basisOk = false;
    uint32_t writes = 0;              // camera lane: seam writes carrying the offset
    uint32_t vpUploads = 0;           // vp lane: c0 uploads patched this present
    uint32_t vpPresents = 0;          // vp lane: presents with at least one patched upload
    uint32_t vpUploadsTotal = 0;
} g_pt;

// ---- the pitchtest (the neck instrument, 41.1) -------------------------------
// Does the ENGINE move its camera when the view pitches? Three buckets of c5
// (the render position): pitch 0, looking UP, looking DOWN, each a mean over
// kPitchFrames presents after kPitchSettle; the travel from the 0 bucket is
// projected on world up and the pitch-0 heading (the per-eye offset along
// right cancels by construction, so it runs under stereo reentry). Read
// beside the seam's own offset: the difference is the engine's own neck, or
// the proof that the seam's arc never reached the draw.
constexpr int kPitchSettle = 30, kPitchFrames = 60, kPitchWaitMax = 30 * 90;
struct Pitchtest {
    bool  active = false;
    float deg = 30.0f;
    int   bucket = 0;                    // 0 = level, 1 = UP, 2 = DOWN
    int   settled = 0, frames = 0, waited = 0;
    double c5Sum[3][3] = {};             // per bucket
    double seamSum[3][3] = {};
    int    n[3] = {0, 0, 0};
    uint32_t clipsAt[3] = {0, 0, 0};     // ceiling clips counted during the bucket
    uint32_t clipsStart = 0;
    float  pitchAt[3] = {0, 0, 0};
    float  f[3] = {1, 0, 0}, u[3] = {0, 0, 1};   // the heading and up at pitch 0
    bool   basisOk = false;
} g_pitch;
float    g_lastBasisF[3] = {1, 0, 0}, g_lastBasisR[3] = {0, 1, 0}, g_lastBasisU[3] = {0, 0, 1};
bool     g_lastBasisOk = false;
uint32_t g_ceilClips = 0;                // presents where the 38.24 ceiling clipped the written position
float    g_ceilClipMaxUu = 0.0f;
float    g_headPitchDeg = 0.0f;          // the tracked head pitch, published per present

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
    const float want = g_et.uu * f.c5Sign;   // what an honoured write moves c5 by
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
        DVR_INFO("camera/eyetest: %s asked %+.1f uu of VIEW travel along right -> c5 moved %+.1f uu "
                 "(expected %+.1f: c5 %s the field here) (mean of %d): %s %d/%d frames (discarded %d, "
                 "other %d, no c5 %d)%s",
                 f.name, g_et.uu, mean, want, f.c5Sign < 0.0f ? "NEGATES" : "follows", g_et.movedN,
                 verdict, !strcmp(verdict, "HONOURED") ? g_et.honoured : g_et.discarded, judged,
                 g_et.discarded, g_et.other, g_et.noWrite,
                 !strcmp(verdict, "HONOURED")
                     ? " - the renderer drew from the offset position: this is the write point "
                       "([Camera] EyeField=)"
                 : !strcmp(verdict, "DISCARDED") ? " - recomputed before the draw" : "");
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
volatile LONG g_c5Serial = 0;
void note_render_pos(const float pos[3]) {
    if (!pos) return;
    g_c5[0] = pos[0]; g_c5[1] = pos[1]; g_c5[2] = pos[2];
    g_c5Ok = true;
    InterlockedIncrement(&g_c5Serial);
}
uint32_t render_pos_serial() { return (uint32_t)InterlockedCompareExchange(&g_c5Serial, 0, 0); }

// The second-pass latch: the thread id inside the re-entered draw (0 = none).
volatile LONG g_secondPassTid = 0;
void set_second_pass(bool on) { InterlockedExchange(&g_secondPassTid, on ? (LONG)GetCurrentThreadId() : 0); }
bool second_pass_for_current_thread() {
    const LONG t = InterlockedCompareExchange(&g_secondPassTid, 0, 0);
    return t != 0 && (DWORD)t == GetCurrentThreadId();
}
// The value c5 should read for the last write (telemetry: the reentry method
// compares it against the c5 the constant hook captured).
bool last_written_pos(float out[3]) {
    if (!g_eyeWriter.lastOk || g_field < 0 || !out) return false;
    const float cs = kFields[g_field].c5Sign;
    for (int i = 0; i < 3; ++i) out[i] = cs * g_eyeWriter.last[i];
    return true;
}
bool render_pos(float out[3]) {
    if (!g_c5Ok || !out) return false;
    out[0] = g_c5[0]; out[1] = g_c5[1]; out[2] = g_c5[2];
    return true;
}

// ---- positional tracking: the offset, the lane, the ceiling ------------------------------
void set_position_offset_uu(float right, float up, float fwd) {
    g_pos[0] = right; g_pos[1] = up; g_pos[2] = fwd;
}

// The offset both lanes read. While the postest runs it is the commanded
// triple (zero during its baseline), whatever the head is doing.
void position_offset_uu(float out[3]) {
    if (g_pt.active) {
        for (int i = 0; i < 3; ++i) out[i] = g_pt.writing ? g_pt.cmd[i] : 0.0f;
        return;
    }
    out[0] = g_pos[0]; out[1] = g_pos[1]; out[2] = g_pos[2];
}

int g_posLaneCfg = -1;   // -1 auto, 0 vp, 1 camera

bool set_pos_lane(const char* name) {
    if (!name) return false;
    int cfg;
    if (!_stricmp(name, "auto")) cfg = -1;
    else if (!_stricmp(name, "vp")) cfg = 0;
    else if (!_stricmp(name, "camera")) cfg = 1;
    else {
        DVR_WARN("camera: unknown positional lane '%s' (auto|vp|camera) - staying on %s", name, pos_lane_name());
        return false;
    }
    if (cfg != g_posLaneCfg) {
        g_posLaneCfg = cfg;
        DVR_INFO("camera: positional tracking lane -> %s (%s)", pos_lane_name(),
                 cfg < 0 ? "auto: the c0 patch on the quad screen, the camera write under a projection layer"
                 : cfg == 1 ? "the lean/crouch/roomscale offset is written into the camera field with the eye "
                              "offset; the c0 patch is off"
                            : "the c0 view-projection patch (LeanVP); the camera write carries the eye offset only");
    }
    return true;
}
PosLane pos_lane() {
    if (g_posLaneCfg < 0) return dvr::stereo::wants_projection() ? PosLane::Camera : PosLane::Vp;
    return g_posLaneCfg == 1 ? PosLane::Camera : PosLane::Vp;
}
const char* pos_lane_name() {
    const PosLane l = pos_lane();
    if (g_posLaneCfg < 0) return l == PosLane::Camera ? "camera (auto)" : "vp (auto)";
    return l == PosLane::Camera ? "camera" : "vp";
}

void set_eye_ceiling(float zMax, bool on) { g_ceilZ = zMax; g_ceilOn = on; }

// ---- the writer (script lane) -----------------------------------------------------------
bool apply_offsets(uint8_t* camObj) {
    if (g_et.active) return false;   // the instrument owns the fields while it runs
    float pos[3];
    position_offset_uu(pos);
    const bool posWanted = pos_lane() == PosLane::Camera || (g_pt.active && g_pt.lane == PosLane::Camera);
    const bool posLive = posWanted && (pos[0] != 0.0f || pos[1] != 0.0f || pos[2] != 0.0f);
    // The eye: the seam's, or +1 inside SequentialReentry's second draw.
    const int eyeNow = second_pass_for_current_thread() ? 1 : g_eye;
    const float eyeUu = (float)eyeNow * 0.5f * g_ipdM * g_scale;
    if (eyeNow == 0 && !posLive) {
        if (g_eyeWriter.lastOk && g_field >= 0) restore(camObj, kFields[g_field].off, g_eyeWriter);
        return false;
    }
    if (g_field < 0) {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
                     "camera: an offset is wanted (eye %+d, position %s) but no eye field is measured - "
                     "run `camera eyetest 100` in gameplay and set [Camera] EyeField= (the "
                     "render stays mono-positioned)", g_eye, posLive ? "live" : "off");
        return false;
    }
    if (!camObj) return false;
    float f[3] = {0, 0, 0}, r[3], u[3] = {0, 0, 0};
    bool haveBasis = false;
    if (posLive) haveBasis = read_basis(camObj, f, r, u);
    if (!haveBasis && !read_right(camObj, r)) return false;
    // Under a projection layer the displacement is the HEAD's, measured in a
    // yaw-only frame: apply it along the camera's heading with world up (Z),
    // never along a pitched forward row or a rolled right row - the
    // compositor's expectation is the play space, not the view.
    float pr[3], pu[3], pf[3];
    if (haveBasis && dvr::stereo::wants_projection()) {
        const float hn = sqrtf(f[0] * f[0] + f[1] * f[1]);
        if (hn > 0.2f) {
            pf[0] = f[0] / hn; pf[1] = f[1] / hn; pf[2] = 0.0f;
            pr[0] = -pf[1]; pr[1] = pf[0]; pr[2] = 0.0f;   // UE3: X forward, Y right, Z up
            if (pr[0] * r[0] + pr[1] * r[1] < 0.0f) { pr[0] = -pr[0]; pr[1] = -pr[1]; }   // keep the camera's handedness
            pu[0] = 0.0f; pu[1] = 0.0f; pu[2] = 1.0f;
            memcpy(f, pf, sizeof(pf)); memcpy(u, pu, sizeof(pu));
            // the eye offset keeps the camera's true right row (r); the position uses pr
        } else {
            memcpy(pr, r, sizeof(pr));
        }
    } else {
        memcpy(pr, r, sizeof(pr));
    }
    if (haveBasis) {
        memcpy(g_lastBasisF, f, sizeof(g_lastBasisF));
        memcpy(g_lastBasisR, r, sizeof(g_lastBasisR));
        memcpy(g_lastBasisU, u, sizeof(g_lastBasisU));
        g_lastBasisOk = true;
    }
    const float sign = kFields[g_field].sign;
    // The displacement in POSITION form (world uu): the eye along right, the
    // lean along the basis when the lane is ours and the basis is measured.
    float off[3];
    for (int i = 0; i < 3; ++i) {
        off[i] = r[i] * eyeUu;
        if (posLive && haveBasis) off[i] += pr[i] * pos[0] + u[i] * pos[1] + f[i] * pos[2];
    }
    // The 38.24 ceiling: the camera may not rise above the capsule top. Cap
    // the position Z the field will hold (the field is base + off in its own
    // sign; the position is sign * that).
    if (g_ceilOn) {
        float base[3];
        if (current_base(camObj, kFields[g_field].off, g_eyeWriter, base)) {
            const float posZ = sign * base[2] + off[2];
            if (posZ > g_ceilZ) {
                // Counted (41.1): a clipped rise is invisible otherwise, and the
                // pitchtest must be able to blame it.
                ++g_ceilClips;
                if (posZ - g_ceilZ > g_ceilClipMaxUu) g_ceilClipMaxUu = posZ - g_ceilZ;
                off[2] -= (posZ - g_ceilZ);
            }
        }
    }
    const float fieldOff[3] = {off[0] * sign, off[1] * sign, off[2] * sign};
    const bool ok = write_offset(camObj, kFields[g_field].off, fieldOff, g_eyeWriter, nullptr);
    if (ok) {
        if (g_pt.active && g_pt.lane == PosLane::Camera && g_pt.writing && haveBasis) {
            memcpy(g_pt.f, f, sizeof(f)); memcpy(g_pt.r, r, sizeof(r)); memcpy(g_pt.u, u, sizeof(u));
            g_pt.basisOk = true;
            ++g_pt.writes;
        }
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 3,
                        "camera: eye %+d (%+.2f uu along right) + position (R%+.1f U%+.1f F%+.1f uu, lane %s%s) "
                        "-> camera+%s as (%.1f %.1f %.1f) (ipd %.4f m, %.0f uu/m)",
                        eyeNow, eyeUu, posLive ? pos[0] : 0.0f, posLive ? pos[1] : 0.0f, posLive ? pos[2] : 0.0f,
                        pos_lane_name(), posLive && !haveBasis ? ", basis NOT measured: position dropped" : "",
                        kFields[g_field].name, off[0], off[1], off[2], g_ipdM, g_scale);
    }
    return ok;
}

// ---- the postest ------------------------------------------------------------------------
bool postest_start(float rr, float uu, float ff) {
    if (g_pt.active) { DVR_WARN("camera/postest: already running"); return false; }
    if (g_et.active) { DVR_WARN("camera/postest: the eyetest owns the field - wait for it"); return false; }
    if (!(fabsf(rr) <= 500.0f && fabsf(uu) <= 500.0f && fabsf(ff) <= 500.0f) || (rr == 0.0f && uu == 0.0f && ff == 0.0f)) {
        DVR_WARN("camera/postest: <R> <U> <F> in uu, each within +/-500 and not all zero (100 = 1 m)");
        return false;
    }
    g_pt = Postest();
    g_pt.active = true;
    g_pt.lane = pos_lane();
    g_pt.cmd[0] = rr; g_pt.cmd[1] = uu; g_pt.cmd[2] = ff;
    DVR_INFO("camera/postest: START lane=%s asked R%+.1f U%+.1f F%+.1f uu: %d presents of c5 baseline at "
             "zero offset, then %d presents with the offset (the tracked head offset is overridden "
             "meanwhile). %s Stand still in gameplay.",
             pos_lane_name(), rr, uu, ff, kPostestBaselineFrames, kPostestFrames,
             g_pt.lane == PosLane::Camera
                 ? "HONOURED = c5 (the draw's camera position) travels by the asked amount along the basis rows."
                 : "On the vp lane c5 cannot see the matrix patch; the verdict counts the presents the patch "
                   "ran on, and the picture (world-6dof.xrs) carries the effect.");
    return true;
}

void postest_stop(const char* why) {
    if (!g_pt.active) return;
    g_pt.active = false;
    DVR_INFO("camera/postest: stopped (%s)", why ? why : "?");
}

void note_vp_applied() { if (g_pt.active && g_pt.writing) ++g_pt.vpUploads; }

void postest_present_tick() {
    if (!g_pt.active) return;
    if (!g_pt.writing) {
        if (g_c5Ok) {
            for (int i = 0; i < 3; ++i) g_pt.baseSum[i] += g_c5[i];
            ++g_pt.baseFrames;
        }
        if (g_pt.baseFrames >= kPostestBaselineFrames) {
            for (int i = 0; i < 3; ++i) g_pt.baseline[i] = (float)(g_pt.baseSum[i] / g_pt.baseFrames);
            g_pt.writing = true;
        } else if (++g_pt.baseWait >= kPostestFrames * 3) {
            DVR_WARN("camera/postest: no c5 readback in %d presents (no scene draw is passing the constant "
                     "hook); stopping", g_pt.baseWait);
            postest_stop("no c5");
        }
        return;
    }
    ++g_pt.frames;
    if (g_pt.lane == PosLane::Camera) {
        if (!g_c5Ok || !g_pt.basisOk) {
            ++g_pt.noC5;
        } else {
            // c5 negates the view's travel on the POV fields, so the measure is
            // taken back into WORLD terms before it is compared with the ask.
            const float cs = g_field >= 0 ? kFields[g_field].c5Sign : 1.0f;
            const float d[3] = {(g_c5[0] - g_pt.baseline[0]) * cs, (g_c5[1] - g_pt.baseline[1]) * cs,
                                (g_c5[2] - g_pt.baseline[2]) * cs};
            g_pt.measSum[0] += d[0] * g_pt.r[0] + d[1] * g_pt.r[1] + d[2] * g_pt.r[2];
            g_pt.measSum[1] += d[0] * g_pt.u[0] + d[1] * g_pt.u[1] + d[2] * g_pt.u[2];
            g_pt.measSum[2] += d[0] * g_pt.f[0] + d[1] * g_pt.f[1] + d[2] * g_pt.f[2];
            ++g_pt.measN;
        }
    } else {
        if (g_pt.vpUploads) { ++g_pt.vpPresents; g_pt.vpUploadsTotal += g_pt.vpUploads; }
        g_pt.vpUploads = 0;
    }
    if (g_pt.frames < kPostestFrames) return;
    g_pt.active = false;
    if (g_pt.lane == PosLane::Camera) {
        float m[3] = {0, 0, 0};
        if (g_pt.measN) for (int i = 0; i < 3; ++i) m[i] = (float)(g_pt.measSum[i] / g_pt.measN);
        bool honoured = g_pt.measN > 0;
        for (int i = 0; i < 3; ++i) {
            const float band = 0.25f * fabsf(g_pt.cmd[i]) + 2.0f;   // +/-25 %, and 2 uu of noise
            if (fabsf(m[i] - g_pt.cmd[i]) > band) honoured = false;
        }
        DVR_INFO("camera/postest: lane=camera asked R%+.1f U%+.1f F%+.1f -> measured R%+.1f U%+.1f F%+.1f uu "
                 "(mean of %d presents, %u seam writes, %d without c5/basis): %s%s",
                 g_pt.cmd[0], g_pt.cmd[1], g_pt.cmd[2], m[0], m[1], m[2], g_pt.measN, g_pt.writes, g_pt.noC5,
                 !g_pt.writes ? "NOT WRITTEN" : honoured ? "HONOURED" : "NOT HONOURED",
                 !g_pt.writes ? " - no seam write carried the offset (no live camera, basis not orthonormal, "
                                "or no eye field)"
                 : honoured ? " - the renderer drew from the offset position: positional tracking can ride "
                              "the camera lane"
                            : " - c5 did not travel by the asked amount (the field is recomputed, or the "
                              "player moved)");
    } else {
        DVR_INFO("camera/postest: lane=vp asked R%+.1f U%+.1f F%+.1f -> the c0 patch ran on %u/%d presents "
                 "(%u uploads): %s - the matrix effect is not in c5; judge it in the picture (world-6dof.xrs)",
                 g_pt.cmd[0], g_pt.cmd[1], g_pt.cmd[2], g_pt.vpPresents, g_pt.frames, g_pt.vpUploadsTotal,
                 g_pt.vpPresents >= (uint32_t)(g_pt.frames * 4 / 5) ? "APPLIED" : "NOT APPLIED");
    }
}

bool postest_active() { return g_pt.active; }

// ---- the pitchtest ----------------------------------------------------------------------
void set_head_pitch_deg(float deg) { g_headPitchDeg = deg; }
bool last_basis(float f[3], float r[3], float u[3]) {
    if (!g_lastBasisOk) return false;
    if (f) memcpy(f, g_lastBasisF, sizeof(g_lastBasisF));
    if (r) memcpy(r, g_lastBasisR, sizeof(g_lastBasisR));
    if (u) memcpy(u, g_lastBasisU, sizeof(g_lastBasisU));
    return true;
}
uint32_t ceiling_clips() { return g_ceilClips; }

static const char* kPitchBucketName[3] = {"LEVEL", "UP", "DOWN"};

bool pitchtest_start(float deg) {
    if (g_pitch.active) { DVR_WARN("camera/pitchtest: already running"); return false; }
    if (g_pt.active || g_et.active) { DVR_WARN("camera/pitchtest: another camera instrument owns the seam - wait for it"); return false; }
    if (!dvr::stereo::wants_projection()) {
        DVR_WARN("camera/pitchtest: refused - needs a projection layer (stereo reentry, or stereo projection on): "
                 "the neck question only exists where the compositor moves the image for the head");
        return false;
    }
    if (!(deg >= 10.0f && deg <= 80.0f)) { DVR_WARN("camera/pitchtest: <deg> must be 10..80 (got %.1f)", deg); return false; }
    g_pitch = Pitchtest();
    g_pitch.active = true;
    g_pitch.deg = deg;
    g_pitch.clipsStart = g_ceilClips;
    DVR_INFO("camera/pitchtest: START +/-%.0f deg: %d presents of c5 at pitch 0 (LEVEL), then at +%.0f (looking UP) "
             "and -%.0f (looking DOWN), %d presents settle each; c5 travel from LEVEL projected on world up and the "
             "pitch-0 heading (the eye offset along right cancels by construction). Drive the head: 'head rot 0 %.0f 0' "
             "at a FIXED position isolates the ENGINE's own neck; 'head pose' on the arc carries the tracked neck. "
             "Stand still in gameplay.",
             deg, kPitchFrames, deg, deg, kPitchSettle, deg);
    return true;
}

void pitchtest_stop(const char* why) {
    if (!g_pitch.active) return;
    g_pitch.active = false;
    DVR_INFO("camera/pitchtest: stopped (%s)", why ? why : "?");
}

static void pitchtest_verdict() {
    Pitchtest& p = g_pitch;
    p.active = false;
    float c5[3][3], seam[3][3];
    for (int b = 0; b < 3; ++b)
        for (int i = 0; i < 3; ++i) {
            c5[b][i] = p.n[b] ? (float)(p.c5Sum[b][i] / p.n[b]) : 0.0f;
            seam[b][i] = p.n[b] ? (float)(p.seamSum[b][i] / p.n[b]) : 0.0f;
        }
    // c5 negates the view's travel on the POV fields (kFields c5Sign): back to WORLD terms.
    const float cs = g_field >= 0 ? kFields[g_field].c5Sign : 1.0f;
    float U[3] = {0, 0, 0}, F[3] = {0, 0, 0};   // travel from LEVEL, per bucket (uu)
    for (int b = 1; b < 3; ++b) {
        const float d[3] = {(c5[b][0] - c5[0][0]) * cs, (c5[b][1] - c5[0][1]) * cs, (c5[b][2] - c5[0][2]) * cs};
        U[b] = d[0] * p.u[0] + d[1] * p.u[1] + d[2] * p.u[2];
        F[b] = d[0] * p.f[0] + d[1] * p.f[1] + d[2] * p.f[2];
    }
    // the seam's own ask, relative to LEVEL (up = [1], forward = [2])
    const float askU[3] = {0, seam[1][1] - seam[0][1], seam[2][1] - seam[0][1]};
    const float askF[3] = {0, seam[1][2] - seam[0][2], seam[2][2] - seam[0][2]};
    const float resU[3] = {0, U[1] - askU[1], U[2] - askU[2]};
    const float resF[3] = {0, F[1] - askF[1], F[2] - askF[2]};
    // the engine's own neck from the residual: eye = pivot + R(pitch) * (0, below, behind)
    //   up(th)  = below*(cos-1) + behind*sin,  fwd(th) = behind*(cos-1) - below*sin
    //   behind = (up(+) - up(-)) / (2 sin th),  below = (fwd(-) - fwd(+)) / (2 sin th)
    const float th = p.deg * 0.0174533f, s2 = 2.0f * sinf(th), c1 = cosf(th) - 1.0f;
    const float behindUu = (resU[1] - resU[2]) / s2, belowUu = (resF[2] - resF[1]) / s2;
    const float consU = (resU[1] + resU[2]) - 2.0f * belowUu * c1;     // 0 when the model fits
    const float consF = (resF[1] + resF[2]) - 2.0f * behindUu * c1;
    const float scale = g_scale > 1.0f ? g_scale : 100.0f;
    // The residual is what the ENGINE added on top of the seam's ask. Three
    // readings: it is nothing (the engine pivots about the camera origin and
    // the seam's arc, if any, is the whole travel); it fits a pivot (the
    // engine has its own neck - and the seam's ask, when there was one, is
    // accounted for inside the fit, so it DID reach the draw); or it fits no
    // pivot (something between the seam and the draw: the ceiling, a stale
    // basis, the player moving). Measured 2026-09-03 on the simulator: the
    // engine's pivot sits 0.32 m below and 0.06 m behind the eyes (17 cm of
    // backward travel at +30 deg), consistency 0.3 uu - the "whole body"
    // pitch the headset reported.
    const bool residualSmall = fabsf(resU[1]) < 1.0f && fabsf(resF[1]) < 1.0f && fabsf(resU[2]) < 1.0f && fabsf(resF[2]) < 1.0f;
    const bool fitsPivot = fabsf(consU) < 3.0f && fabsf(consF) < 3.0f;
    const uint32_t clips = g_ceilClips - p.clipsStart;
    const char* verdict =
        !p.n[0] || !p.n[1] || !p.n[2] ? "INCOMPLETE (a bucket never arrived: drive the head to both pitches)"
        : residualSmall ? "ENGINE PIVOTS ABOUT THE CAMERA ORIGIN (residual < 1 uu): the seam's ask, if any, is the whole travel"
        : fitsPivot     ? "ENGINE HAS ITS OWN NECK (the residual fits a pivot; the seam's ask is inside the fit, so it reached the draw): "
                          "with positional tracking the tracked arc rides on top of it - `neck cancel <below> <behind>` with THESE numbers "
                          "cancels the engine's, then re-run: the travel must read the seam's ask alone"
                        : "THE RESIDUAL FITS NO PIVOT: something between the seam and the draw (the ceiling clips, a stale basis, "
                          "the player moving) - read the bucket lines";
    DVR_INFO("camera/pitchtest: buckets LEVEL n=%d pitch %+.1f | UP n=%d pitch %+.1f | DOWN n=%d pitch %+.1f | c5 LEVEL (%.1f %.1f %.1f)",
             p.n[0], p.pitchAt[0], p.n[1], p.pitchAt[1], p.n[2], p.pitchAt[2], c5[0][0], c5[0][1], c5[0][2]);
    DVR_INFO("camera/pitchtest: c5 travel from LEVEL: UP -> U%+.2f F%+.2f uu (seam asked U%+.2f F%+.2f), DOWN -> U%+.2f F%+.2f uu "
             "(seam asked U%+.2f F%+.2f) | seam-to-c5 residual UP U%+.2f F%+.2f / DOWN U%+.2f F%+.2f uu | engine neck solved from "
             "the residual: below %.3f m behind %.3f m (consistency %.2f / %.2f uu, 0 = the model fits) | ceiling clipped %lu "
             "presents (max %.1f uu) | %s",
             U[1], F[1], askU[1], askF[1], U[2], F[2], askU[2], askF[2], resU[1], resF[1], resU[2], resF[2],
             belowUu / scale, behindUu / scale, consU, consF, (unsigned long)clips, g_ceilClipMaxUu, verdict);
    // the picture prediction for the configured arc, so the dump can fail it
    {
        const float b = 0.11f, fwd = 0.09f;   // the [Neck] defaults (the lever prints its own)
        const float upM = b * (cosf(th) - 1.0f) + fwd * sinf(th), fwdM = fwd * (cosf(th) - 1.0f) - b * sinf(th);
        const float screenUp = upM * cosf(th) + fwdM * sinf(th);     // travel across the view axis
        const float halfH = g_fovDeg > 1.0f ? g_fovDeg * 0.5f * 0.0174533f : 0.0f;
        const float pxPerRad = halfH > 0.0f ? (float)dvr::capture::width() / (2.0f * tanf(halfH)) : 0.0f;
        DVR_INFO("camera/pitchtest: picture prediction at %ux%u, claim %.1f deg (%.0f px/rad): an eye on a %.2f/%.2f m arc at "
                 "+%.0f deg travels %.3f m across the view axis, so a landmark D metres away moves DOWN %.1f/D px in the UP frame "
                 "(a 2 m landmark: %.1f px); compare `dump eyes` at the SAME pitch with and without the arc",
                 dvr::capture::width(), dvr::capture::height(), g_fovDeg, pxPerRad, b, fwd, p.deg, fabsf(screenUp),
                 fabsf(screenUp) * pxPerRad, fabsf(screenUp) * pxPerRad / 2.0f);
    }
}

void pitchtest_present_tick() {
    if (!g_pitch.active) return;
    Pitchtest& p = g_pitch;
    const float deg = g_headPitchDeg;
    const bool here = p.bucket == 0 ? fabsf(deg) < 2.0f
                    : p.bucket == 1 ? deg > p.deg - 2.0f
                                    : deg < -(p.deg - 2.0f);
    if (!here) {
        p.settled = 0;
        if (++p.waited >= kPitchWaitMax) {
            DVR_WARN("camera/pitchtest: the %s bucket never arrived (head pitch %+.1f deg, wanted %s) - stopping",
                     kPitchBucketName[p.bucket], deg, p.bucket == 0 ? "|pitch| < 2" : p.bucket == 1 ? "> +deg-2" : "< -(deg-2)");
            pitchtest_verdict();
        } else if ((p.waited % 180) == 1) {
            DVR_INFO("camera/pitchtest: waiting for the %s bucket (head pitch %+.1f deg now)", kPitchBucketName[p.bucket], deg);
        }
        return;
    }
    if (p.settled < kPitchSettle) { ++p.settled; return; }
    if (!g_c5Ok) return;
    if (p.bucket == 0 && !p.basisOk) {
        if (!last_basis(p.f, nullptr, p.u)) {
            if ((p.waited++ % 180) == 1)
                DVR_WARN("camera/pitchtest: no camera basis yet (the seam has not written: positional tracking off, or "
                         "no live camera) - the heading cannot be read");
            return;
        }
        p.basisOk = true;
    }
    float seam[3];
    position_offset_uu(seam);
    for (int i = 0; i < 3; ++i) { p.c5Sum[p.bucket][i] += g_c5[i]; p.seamSum[p.bucket][i] += seam[i]; }
    p.pitchAt[p.bucket] = deg;
    if (++p.n[p.bucket] >= kPitchFrames) {
        p.clipsAt[p.bucket] = g_ceilClips;
        DVR_INFO("camera/pitchtest: bucket %s pitch %+.1f deg: c5 mean (%.1f %.1f %.1f) over %d presents, seam offset "
                 "R%+.2f U%+.2f F%+.2f uu, ceiling clips so far %lu",
                 kPitchBucketName[p.bucket], deg, p.c5Sum[p.bucket][0] / p.n[p.bucket], p.c5Sum[p.bucket][1] / p.n[p.bucket],
                 p.c5Sum[p.bucket][2] / p.n[p.bucket], p.n[p.bucket], p.seamSum[p.bucket][0] / p.n[p.bucket],
                 p.seamSum[p.bucket][1] / p.n[p.bucket], p.seamSum[p.bucket][2] / p.n[p.bucket],
                 (unsigned long)(g_ceilClips - p.clipsStart));
        if (p.bucket == 2) { pitchtest_verdict(); return; }
        ++p.bucket;
        p.settled = 0; p.waited = 0;
        DVR_INFO("camera/pitchtest: now drive the head to %s (%s)", kPitchBucketName[p.bucket],
                 p.bucket == 1 ? "head rot 0 <deg> 0, or head pose on the arc" : "head rot 0 -<deg> 0, or head pose on the arc");
    }
}

bool pitchtest_active() { return g_pitch.active; }

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
        // c5 answers a wanted +uu of VIEW travel with uu * c5Sign (it is the
        // negated camera position on the POV fields - measured by picture,
        // 2026-09-03).
        const float want = g_et.uu * kFields[g_et.idx].c5Sign;
        const float band = 0.25f * g_et.uu;
        if (fabsf(moved - want) <= band) ++g_et.honoured;
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
    w.kv("posLane", pos_lane_name());
    w.kv("posRightUu", (double)g_pos[0]); w.kv("posUpUu", (double)g_pos[1]); w.kv("posFwdUu", (double)g_pos[2]);
    w.kv("ceilingOn", g_ceilOn);
    w.kv("ceilClips", (unsigned long)g_ceilClips);
    w.kv("basisBad", (unsigned long)g_basisBad);
    w.kv("postest", g_pt.active);
    w.kv("pitchtest", g_pitch.active);
    w.kv("headPitchDeg", (double)g_headPitchDeg);
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
             "postrack lane=%s (R%+.1f U%+.1f F%+.1f uu) ceiling=%s clips=%lu basisBad=%lu | "
             "fov lever=%.0f rendered=%.1f | c5=%s(%.1f %.1f %.1f) | eyetest=%s postest=%s pitchtest=%s (head pitch %+.1f deg)",
             g_eye, g_ipdM, g_scale, eye_offset_uu(), eye_field(), (unsigned long)g_eyeWriter.writes,
             pos_lane_name(), g_pos[0], g_pos[1], g_pos[2], g_ceilOn ? "on" : "off", (unsigned long)g_ceilClips,
             (unsigned long)g_basisBad,
             g_fovDeg, g_renderedFov, g_c5Ok ? "" : "none ", g_c5[0], g_c5[1], g_c5[2],
             g_et.active ? kFields[g_et.idx].name : "idle", g_pt.active ? "running" : "idle",
             g_pitch.active ? "running" : "idle", g_headPitchDeg);
}

} // namespace dvr::camera
