// game/dishonored/camera.cpp - see camera.h.
#define DVR_CAT ::dvr::log::Cat::head
#include "game/dishonored/camera.h"

#include "core/framework/status.h"
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
bool last_written_pos(float out[3]) {
    if (!g_eyeWriter.lastOk || g_field < 0 || !out) return false;
    const float sign = kFields[g_field].sign;
    for (int i = 0; i < 3; ++i) out[i] = sign * g_eyeWriter.last[i];
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
            if (posZ > g_ceilZ) off[2] -= (posZ - g_ceilZ);
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
            const float d[3] = {g_c5[0] - g_pt.baseline[0], g_c5[1] - g_pt.baseline[1], g_c5[2] - g_pt.baseline[2]};
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
    w.kv("posLane", pos_lane_name());
    w.kv("posRightUu", (double)g_pos[0]); w.kv("posUpUu", (double)g_pos[1]); w.kv("posFwdUu", (double)g_pos[2]);
    w.kv("ceilingOn", g_ceilOn);
    w.kv("basisBad", (unsigned long)g_basisBad);
    w.kv("postest", g_pt.active);
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
             "postrack lane=%s (R%+.1f U%+.1f F%+.1f uu) ceiling=%s basisBad=%lu | "
             "fov lever=%.0f rendered=%.1f | c5=%s(%.1f %.1f %.1f) | eyetest=%s postest=%s",
             g_eye, g_ipdM, g_scale, eye_offset_uu(), eye_field(), (unsigned long)g_eyeWriter.writes,
             pos_lane_name(), g_pos[0], g_pos[1], g_pos[2], g_ceilOn ? "on" : "off", (unsigned long)g_basisBad,
             g_fovDeg, g_renderedFov, g_c5Ok ? "" : "none ", g_c5[0], g_c5[1], g_c5[2],
             g_et.active ? kFields[g_et.idx].name : "idle", g_pt.active ? "running" : "idle");
}

} // namespace dvr::camera
