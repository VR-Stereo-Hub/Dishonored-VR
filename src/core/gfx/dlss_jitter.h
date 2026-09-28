// core/gfx/dlss_jitter.h - deliberate sub-pixel projection jitter for DLSS (PERFORMANCE.md,
// "Projection jitter"). Default OFF; `dlss jitter on|off`, [Clarity] DlssJitter, F10.
//
// DLSS rebuilds detail from sample positions that move between frames. Without this the only
// movement is the head's own; with it every world draw of an eye image is shifted by a known
// sub-pixel offset (Halton 2,3), and that offset travels with the image to DLSS.
//
// WHERE. The world view-projection is c0..c3 (vs_const_hook.cpp). It is re-uploaded per pass
// and per object, for shadow maps, reflections and UI as well, so a c0 upload is not by itself
// a world draw. A DEPTH SURFACE is the world's only once the world pass has been OBSERVED using
// it: the depth-stencil surface bound when the c5-tied view-projection arrives (the same tie that
// makes the recorded matrix the world one), with a viewport the size of the eye image the capture
// delivers. Jitter is applied to every perspective (not affine) c0..c3 upload while that depth
// surface is bound, whatever colour target is bound with it: every pass that writes or tests the
// scene depth must carry the same shift (the first version keyed on the colour target, and a
// depth-writing pass into another eye-size target stayed unshifted - whole surfaces failed their
// depth test in the left eye; PERFORMANCE.md). Colour and the scene depth (its alpha) move
// together, the first-person weapon too. Shadow maps, reflection captures and downsampled passes
// (their own depth), HUD/Scaleform (affine) and our own D3D11 passes are never touched.
// Unconfirmed = refused, logged with the values.
//
// THE PAIR. The offset for the draws between two presents is fixed at the earlier present, so
// each present's image has exactly one offset. It is stored in that image's pose record
// (pose::note_render_jitter), next to the matrix, so the pair cannot come apart. Both eyes of a
// stereo pair (one pose pairId) use the same phase; the phase advances after the pair.
//
// THE VECTORS. The recorded matrices are the game's own (unjittered: the hook records before it
// patches), so the motion vectors exclude the jitter, as DLSS requires. The flow check subtracts
// the recorded jitter change and reports how much of the image moved by it (jitter gain).
#pragma once
#include <stdint.h>

namespace dvr::dlss::jitter {

// Halton radical inverse, index >= 1.
inline float halton(uint32_t i, uint32_t b) {
    float f = 1.0f, r = 0.0f;
    while (i) { f /= (float)b; r += f * (float)(i % b); i /= b; }
    return r;
}
// Phase i of an n-phase sequence: the sample offset, render pixels, [-0.5, 0.5), x right, y down.
// Pixel (x, y) of a jittered image shows the scene at (x + 0.5 + sx, y + 0.5 + sy).
inline void phase_offset(uint32_t i, uint32_t n, float* sx, float* sy) {
    const uint32_t k = (n ? i % n : 0) + 1;
    *sx = halton(k, 2) - 0.5f;
    *sy = halton(k, 3) - 0.5f;
}
// The clip-space change that makes a w x h viewport sample at that offset: clip.x += ax * clip.w,
// clip.y += ay * clip.w (row vector: column 0 += ax * column 3, column 1 += ay * column 3). The
// content moves by minus the offset; NDC y is up, pixel y is down.
inline void ndc_shift(float sx, float sy, uint32_t w, uint32_t h, float* ax, float* ay) {
    *ax = w ? -2.0f * sx / (float)w : 0.0f;
    *ay = h ? 2.0f * sy / (float)h : 0.0f;
}
// Applies that shift to rows [first, last) of an uploaded c0..c3 block whose first row is
// register `start` (4 floats per row). Each row needs only its own w element, so a partial
// upload is patched exactly as a whole one.
inline void shift_rows(float* block, uint32_t start, uint32_t first, uint32_t last, float ax, float ay) {
    for (uint32_t r = first; r < last; ++r) {
        float* row = block + (r - start) * 4;
        row[0] += ax * row[3];
        row[1] += ay * row[3];
    }
}
// What DLSS receives as InJitterOffsetX/Y for a sample offset: its NEGATION on both axes (DLSS
// reads the offset as where the content moved, not where the sample went). PROVED, not assumed,
// by tools/dlss-host-tests.cpp ("projection jitter sign"), 512 -> 768 Quality, 18 phases: error
// -x-y 0.0043, -x+y 0.0119, +x+y 0.0198, +x-y 0.0231, no jitter 0.0220.
const float kReportX = -1.0f, kReportY = -1.0f;
// FSR through the FidelityFX API reads the offset and the motion vectors in its own convention;
// these are what tools/dlss-host-tests.cpp proved for it (the same tests as DLSS, FSR backend).
const float kFsrReportX = -1.0f, kFsrReportY = -1.0f;
const float kFsrMvSign = 1.0f;

// ---- the mod side (dlss_jitter.cpp) -----------------------------------------
void set_enabled(bool on, const char* who);
bool enabled();
// The wide rule (default ON): also shift perspective draws into any eye-size colour target whatever
// depth surface is bound. `dlss jitter wide on|off`, [Clarity] DlssJitterWide, F10.
void set_wide(bool on, const char* who);
bool wide();

// Render thread (the D3D9 hooks).
void note_render_target(const void* rt, uint32_t w, uint32_t h);   // SetRenderTarget(0, rt)
void note_depth_stencil(const void* ds);                           // SetDepthStencilSurface(ds)
void note_world_pass(uint32_t vpW, uint32_t vpH);                  // at the c5-tied view-projection, with the viewport
// For a c0..c3 upload: true with the NDC shift when the bound target is a confirmed world target
// and jitter is live. `perspective` false (affine: 2D, post-process, HUD) is counted, never shifted.
bool shift_for_upload(bool perspective, float* ax, float* ay);

// Present thread, once per tagged present (reentry.cpp), after the record got its c5. Stores the
// offset the image was drawn with (0 if nothing was shifted) and picks the next one.
void on_present(uint32_t recId, uint32_t capW, uint32_t capH);
const char* summary();   // one line for F10 / status

} // namespace dvr::dlss::jitter
