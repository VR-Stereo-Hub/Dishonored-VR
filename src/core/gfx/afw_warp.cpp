// core/gfx/afw_warp.cpp - see afw_warp.h.
#define DVR_CAT ::dvr::log::Cat::d3d
#include "core/gfx/afw_warp.h"

#include "core/gfx/clarity.h"
#include "core/gfx/clarity_math.h"
#include "core/gfx/depth_probe.h"
#include "core/util/log.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <atomic>
#include <initializer_list>
#include <cmath>
#include <stdio.h>
#include <string.h>

namespace dvr::afw {
void hand_motion(const HandPose& a, const HandPose& b, float out[5][4]);   // run 25: defined with the MSW code, used by warp_held
namespace {

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

// Three passes per rebuild.
//
// 1-2 SEED MAPS (vsmesh/psmesh), one per source image: a coarse grid over the source's depth is
//     carried into the TARGET view (the held eye at the fresh generation) and rasterised with a depth
//     test, so every target texel learns which source point is NEAREST there - thin and near objects
//     included, which no fixed seed depth can promise. Triangles that span a depth break (a stretched
//     sheet across a disocclusion) are flagged by how little source they cover per target texel.
// 3   COMPOSE (psmain), full size: each source's seed is refined by a few fixed-point steps through
//     its depth, and the two candidates are compared in TARGET-view depth:
//       fresh, nearer than the body threshold -> the hands/weapon NOW (both eyes, one instant)
//       held, consistent, not stale, not behind a nearer fresh surface -> the world from this eye's
//            own viewpoint (true stereo, its own shading)
//       fresh -> the rest (what moved, what the held image could not see)
//     "Stale": the held point carried into the fresh view lies in FRONT of what the fresh eye sees
//     there - the fresh eye sees through it, so it moved (an old hand, a weapon, an NPC edge).
//
// Rows are float4 with w unused. s/f rows map the held/fresh image's view space to tracking space
// (R); d rows map tracking space into the target view (R^T); y is the world's yaw since the held
// image about yc. The held eye's world moves by the game's own matrices when both images carry them
// (prm2.w = uu per depth unit > 0): hI/hC invert the held image's clip rows (x, y, w as linear
// forms of the camera-relative point), tA/tB/tW project into the target, mD = held camera minus
// target camera (uu). Otherwise by the XR poses and the body yaw (walking then lags a tick).
const char* kSrc =
    "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut vsmain(uint id : SV_VertexID) {\n"
    "    VSOut o; float2 uv = float2((id << 1) & 2, id & 2);\n"
    "    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0); o.uv = uv; return o;\n"
    "}\n"
    "Texture2D heldTex : register(t0);\n"
    "Texture2D heldDepth : register(t1);\n"
    "Texture2D freshTex : register(t2);\n"
    "Texture2D freshDepth : register(t3);\n"
    "Texture2D seedF : register(t4);\n"
    "Texture2D seedH : register(t5);\n"
    // Run 15: each eye's GAME image before the mod's own layers (objective markers, the aim laser, the F10 panel)
    // were drawn on it. The hands and weapon taken from the fresh eye come from ITS clean image, or the fresh eye's
    // UI rides the sword into the held eye; the held eye's own UI is laid back where heldTex and heldClean differ.
    "Texture2D freshClean : register(t6);\n"
    "Texture2D heldClean : register(t7);\n"
    "SamplerState linSamp : register(s0);\n"
    "SamplerState pointSamp : register(s1);\n"
    "cbuffer P : register(b0) {\n"
    "    float4 s0, s1, s2, sp;\n"      // held image: R rows, position
    "    float4 f0, f1, f2, fp;\n"      // fresh image: R rows, position
    "    float4 d0, d1, d2, dp;\n"      // target: R^T rows, position
    "    float4 y0, y1, y2, yc;\n"      // world yaw rows, centre
    "    float4 prm;\n"                  // tanH, tanV, metres per depth unit, body threshold (units)
    "    float4 prm2;\n"                 // target w, h, tolerance (target texels), uu per unit (0 = matrices off)
    "    float4 prm3;\n"                 // stereo on, temporal on, debug tint, stale tolerance (relative)
    "    float4 hI0, hI1, hI2, hC;\n"    // held clip rows inverted; their constants (A.w, B.w, W.w)
    "    float4 tA, tB, tW;\n"           // target clip rows
    "    float4 mD;\n"                   // held camera minus target camera (uu)
    "    float4 prm4;\n"                 // one grid step in uv: fresh x, y, held x, y
    "    float4 prm5;\n"                 // the foreground's tanH, tanV, its depth limit (units; 0 = one projection)
    "    float4 prm6;\n"                 // own hands on, their colour agreement limit (0..1)
    "    float4 hRa0, hRa1, hRa2, hOa, hNa;\n"   // MSW: hand a's rotation since the image (rows), its grip then, now
    "    float4 hRb0, hRb1, hRb2, hOb, hNb;\n"   // hand b's (hOx.w > 0.5: that hand is tracked)
    "    float4 prm7;\n"                 // MSW: the hands follow their controllers, the world turns in the image (matrices), the foreground ignores the world yaw
    "    float4 mY0, mY1, mY2;\n"        // MSW: the extrapolated body turn, as a rotation of the camera-relative point (UE world axes)
    "    float4 prm8;\n"                 // run 15: fresh clean bound, held clean bound, UI difference threshold (0..1)
    "    float4 prm9;\n"                 // run 22: edge hands on, the edge band (uv from each side)
    "};\n"
    "cbuffer M : register(b1) {\n"
    "    float4 mp;\n"                   // source (0 fresh, 1 held), grid step (source texels), source w, h
    "    float4 mp2;\n"                  // cells per row, seed map w, h, stretch area threshold
    "};\n"
    "float3 viewDir(float2 uv) { float2 n = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0); return float3(n.x * prm.x, n.y * prm.y, -1.0); }\n"
    "float2 ndcUV(float2 n) { return float2(n.x * 0.5 + 0.5, 0.5 - n.y * 0.5); }\n"
    "float3 mr(float4 a, float4 b, float4 c, float3 v) { return float3(dot(a.xyz, v), dot(b.xyz, v), dot(c.xyz, v)); }\n"
    "float3 mc(float4 a, float4 b, float4 c, float3 v) { return a.xyz * v.x + b.xyz * v.y + c.xyz * v.z; }\n"
    // Signed depth: NEGATIVE marks a texel the foreground pass drew (the mask, when prm6.z is on); 0 is the sky.
    "float zH(float2 uv) { float z = heldDepth.SampleLevel(pointSamp, uv, 0).r; return z != 0.0 ? z : 60000.0; }\n"
    "float zF(float2 uv) { float z = freshDepth.SampleLevel(pointSamp, uv, 0).r; return z != 0.0 ? z : 60000.0; }\n"
    "float aH(float2 uv) { return abs(zH(uv)); }\n"
    "float aF(float2 uv) { return abs(zF(uv)); }\n"
    // Is this (signed) depth the foreground? The mask when on, else the old depth limit.
    "bool isFg(float z) { return prm6.z > 0.5 ? z < 0.0 : (prm5.z > 0.0 && abs(z) < prm5.z); }\n"
    // The foreground (the player's arms and weapon) is drawn with the game camera's own FOV, the world
    // with the mod's projection: a pixel nearer than the foreground limit is a ray of the foreground's
    // tangents, in its source AND in the target (the target eye draws it the same way).
    "float2 tanFor(float z) { return (prm5.x > 0.0 && isFg(z)) ? prm5.xy : prm.xy; }\n"
    "float3 viewDirT(float2 uv, float2 tn) { float2 n = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0); return float3(n.x * tn.x, n.y * tn.y, -1.0); }\n"
    // A target-view point (metres) to (ndc x, ndc y, depth units); behind the eye -> far off-screen.
    "float3 toTT(float3 L, float2 tn) { float zt = -L.z; if (!(zt > 1e-5)) return float3(9, 9, -1);\n"
    "    return float3(L.x / (zt * tn.x), L.y / (zt * tn.y), zt / prm.z); }\n"
    "float3 toT(float3 L) { return toTT(L, prm.xy); }\n"
    "float3 mapF(float2 s, float z) { float2 tn = tanFor(z); z = abs(z);\n"
    "    return toTT(mr(d0, d1, d2, mr(f0, f1, f2, viewDirT(s, tn) * (z * prm.z)) + fp.xyz - dp.xyz), tn); }\n"
    // MSW: a foreground point moves rigidly with the nearer controller's grip (from the image's pose to the slot's).
    "float3 handMove(float3 W) {\n"
    "    float da = hOa.w > 0.5 ? dot(W - hOa.xyz, W - hOa.xyz) : 1e9, db = hOb.w > 0.5 ? dot(W - hOb.xyz, W - hOb.xyz) : 1e9;\n"
    // Only within 30 cm of a grip (prm7.w, squared metres): without the foreground mask the foreground is "nearer than
    // the depth limit" (about 0.7 m), which takes in nearby walls and tables - run 11: they moved with the hands.
    "    if (min(da, db) > prm7.w) return W;\n"
    "    if (da <= db) { float3 r = W - hOa.xyz; return float3(dot(hRa0.xyz, r), dot(hRa1.xyz, r), dot(hRa2.xyz, r)) + hNa.xyz; }\n"
    "    float3 r = W - hOb.xyz; return float3(dot(hRb0.xyz, r), dot(hRb1.xyz, r), dot(hRb2.xyz, r)) + hNb.xyz;\n"
    "}\n"
    "float3 mapHx(float2 s, float z, bool world) {\n"
    "    float2 tn = tanFor(z); bool fgp = isFg(z); z = abs(z);\n"
    "    float3 W = mr(s0, s1, s2, viewDirT(s, tn) * (z * prm.z)) + sp.xyz;\n"
    "    if (prm7.x > 0.5 && fgp) W = handMove(W);\n"
    "    else if (world && !(prm7.z > 0.5 && fgp)) W = mr(y0, y1, y2, W - yc.xyz) + yc.xyz;\n"
    "    return toTT(mr(d0, d1, d2, W - dp.xyz), tn);\n"
    "}\n"
    "float3 mapH(float2 s, float z) {\n"
    "    if (prm2.w > 0.0 && !isFg(z)) {\n"
    "        float2 n = float2(s.x * 2.0 - 1.0, 1.0 - s.y * 2.0); float w = abs(z) * prm2.w;\n"
    "        float3 P = mr(hI0, hI1, hI2, float3(n.x * w - hC.x, n.y * w - hC.y, w - hC.z)) + mD.xyz;\n"
    "        if (prm7.y > 0.5) P = mr(mY0, mY1, mY2, P);\n"
    "        float cw = dot(tW.xyz, P) + tW.w; if (!(cw > 1e-3)) return float3(9, 9, -1);\n"
    "        return float3((dot(tA.xyz, P) + tA.w) / cw, (dot(tB.xyz, P) + tB.w) / cw, cw / prm2.w);\n"
    "    }\n"
    "    return mapHx(s, z, true);\n"
    "}\n"
    // Refine a seed: fixed-point steps through the source's depth. Out: source uv, target depth, error (target texels).
    "float2 refineF(float2 t, float2 s, out float zt, out float err) {\n"
    "    float3 m;\n"
    "    [unroll] for (int i = 0; i < 3; ++i) { m = mapF(s, zF(s)); s = saturate(s + (t - ndcUV(m.xy))); }\n"
    "    m = mapF(s, zF(s)); zt = m.z; err = m.z > 0 ? length((ndcUV(m.xy) - t) * prm2.xy) : 1e9; return s;\n"
    "}\n"
    "float2 refineH(float2 t, float2 s, out float zt, out float err) {\n"
    "    float3 m;\n"
    "    [unroll] for (int i = 0; i < 3; ++i) { m = mapH(s, zH(s)); s = saturate(s + (t - ndcUV(m.xy))); }\n"
    "    m = mapH(s, zH(s)); zt = m.z; err = m.z > 0 ? length((ndcUV(m.xy) - t) * prm2.xy) : 1e9; return s;\n"
    "}\n"
    // The body hypothesis (no fresh depth only): the held image's near content fixed in tracking space.
    "float2 solveHb(float2 t, out float zt, out float err) {\n"
    "    float2 s = t; float3 m;\n"
    "    [unroll] for (int i = 0; i < 4; ++i) { m = mapHx(s, zH(s), false); s = saturate(s + (t - ndcUV(m.xy))); }\n"
    "    m = mapHx(s, zH(s), false); zt = m.z; err = m.z > 0 ? length((ndcUV(m.xy) - t) * prm2.xy) : 1e9; return s;\n"
    "}\n"
    // An edge rescue: a seed from a sheet (a grid cell across a depth break) sits between the two sides,
    // and a search from it that lands FARTHER than the seed said slid off the near side: the nearest of
    // its four grid neighbours is the near side, searched as a second candidate.
    "float2 nearF(float2 s) { float2 b = s; float bz = aF(s); float2 o = prm4.xy;\n"
    "    float2 c[4] = { s + float2(o.x, 0), s - float2(o.x, 0), s + float2(0, o.y), s - float2(0, o.y) };\n"
    "    [unroll] for (int k = 0; k < 4; ++k) { float z = aF(c[k]); if (z < bz) { bz = z; b = c[k]; } } return b; }\n"
    "float2 nearH(float2 s) { float2 b = s; float bz = aH(s); float2 o = prm4.zw;\n"
    "    float2 c[4] = { s + float2(o.x, 0), s - float2(o.x, 0), s + float2(0, o.y), s - float2(0, o.y) };\n"
    "    [unroll] for (int k = 0; k < 4; ++k) { float z = aH(c[k]); if (z < bz) { bz = z; b = c[k]; } } return b; }\n"
    // Debug tint: green fresh near (hands now), none held world, blue fresh world, red the held fallback,
    // magenta the disocclusion fill, cyan the nearer near-miss, yellow the temporal body hypothesis.
    // zt: the chosen candidate's depth in the TARGET view (units), kept for the second output (the depth layer).
    "static float g_outZ = 0.0;\n"
    "static bool g_fromFresh = false;\n"
    "float4 shade(Texture2D tex, float2 uv, int cls, float zt) {\n"
    "    g_outZ = zt;\n"
    "    float3 c = tex.SampleLevel(linSamp, uv, 0).rgb;\n"
    "    if (prm3.z > 0.5) {\n"
    "        float3 k = cls == 0 ? float3(0.5, 1.0, 0.5) : cls == 2 ? float3(0.5, 0.6, 1.0) : cls == 3 ? float3(1.0, 0.4, 0.4)\n"
    "                 : cls == 4 ? float3(1.0, 1.0, 0.4) : cls == 5 ? float3(1.0, 0.4, 1.0) : cls == 6 ? float3(0.4, 1.0, 1.0) : cls == 7 ? float3(1.0, 0.7, 0.3)\n"
    "                 : float3(1.0, 1.0, 1.0);\n"
    "        c *= k;\n"
    "    }\n"
    "    return float4(c, 1.0);\n"
    "}\n"
    // Everything taken from the fresh eye: its clean game image when one is bound.
    "float4 shadeF(float2 uv, int cls, float zt) { g_fromFresh = true;\n"
    "    return prm8.x > 0.5 ? shade(freshClean, uv, cls, zt) : shade(freshTex, uv, cls, zt); }\n"
    // A true disocclusion (no source shows this point): extend the BACKGROUND, never the near object -
    // the farthest covered seed within 128 texels along the row (the stereo and turn baselines are
    // horizontal), either map. A thin structure no longer reaches here (the near-miss rule above takes
    // it); a "closest clearly-behind seed" rule was tried and ghosted the hands in the host test.
    // A weapon can be geometrically behind a wall while drawing on top. Farthest alone then
    // selects the weapon as background and stretches a second copy across the disocclusion.
    // Only world samples may extend the background; foreground identity outranks metric depth.
    "float4 fill(float2 t, bool st, bool tp) {\n"
    "    float best = -1.0; float2 bs = t; bool fromF = st;\n"
    "    [unroll] for (int k = 0; k < 6; ++k) {\n"
    "        float off = (float)(4 << k) / prm2.x;\n"
    "        [unroll] for (int sd = -1; sd <= 1; sd += 2) {\n"
    "            float2 p = float2(t.x + sd * off, t.y);\n"
    "            if (p.x < 0.0 || p.x > 1.0) continue;\n"
    "            if (st) { float4 a = seedF.SampleLevel(pointSamp, p, 0); if (a.a > 0 && a.z > best && !isFg(zF(a.xy))) { best = a.z; bs = a.xy; fromF = true; } }\n"
    "            if (tp) { float4 b = seedH.SampleLevel(pointSamp, p, 0); if (b.a > 0 && b.z > best && !isFg(zH(b.xy))) { best = b.z; bs = b.xy; fromF = false; } }\n"
    "        }\n"
    "    }\n"
    "    float zf = best > 0.0 ? best / max(1.0 - best, 1e-5) : 60000.0;\n"
    "    return fromF ? shadeF(bs, 5, zf) : shade(heldTex, bs, 5, zf);\n"
    "}\n"
    "float4 compose(VSOut i) {\n"
    "    float2 t = i.uv;\n"
    "    float tol = prm2.z, body = prm.w;\n"
    "    bool st = prm3.x > 0.5, tp = prm3.y > 0.5;\n"
    "    float tF = 1e9, eF = 1e9, tH = 1e9, eH = 1e9;\n"
    "    float2 sF = t, sH = t;\n"
    "    float4 kF = 0, kH = 0;\n"
    "    if (st) { kF = seedF.SampleLevel(pointSamp, t, 0); sF = refineF(t, kF.a > 0 ? kF.xy : t, tF, eF);\n"
    "        if (kF.a > 0 && (kF.a < 0.75 || tF > kF.z / max(1.0 - kF.z, 1e-4) * 1.05 + 0.01)) { float ta, ea; float2 sa = refineF(t, nearF(kF.xy), ta, ea);\n"
    "            if (ea < prm2.z && (!(eF < prm2.z) || ta < tF)) { sF = sa; tF = ta; eF = ea; } } }\n"
    "    if (tp) { kH = seedH.SampleLevel(pointSamp, t, 0); sH = refineH(t, kH.a > 0 ? kH.xy : t, tH, eH);\n"
    "        if (kH.a > 0 && (kH.a < 0.75 || tH > kH.z / max(1.0 - kH.z, 1e-4) * 1.05 + 0.01)) { float ta, ea; float2 sa = refineH(t, nearH(kH.xy), ta, ea);\n"
    "            if (ea < prm2.z && (!(eH < prm2.z) || ta < tH)) { sH = sa; tH = ta; eH = ea; } } }\n"
    "    bool okF = eF < tol, okH = eH < tol;\n"
    // Which candidates are the player's own arms and weapon: the mask where it is on, else nearer than the body limit.
    "    bool bF = prm6.z > 0.5 ? (st && zF(sF) < 0.0) : tF < body;\n"
    "    bool bH = prm6.z > 0.5 ? (tp && zH(sH) < 0.0) : tH < body;\n"
    // The hands and weapon from the held eye's OWN image where they have not moved: its own shading (a
    // blade's highlight is view-dependent, and the other eye's every other frame reads as a shimmer). Held
    // as fixed in tracking space (the body hypothesis), accepted only where the fresh eye puts the same
    // surface at the same depth now AND the colours agree; anything moving takes the fresh eye as before.
    "    if (st && tp && prm6.x > 0.5 && okF && bF) {\n"
    "        float zb, eb; float2 sb = solveHb(t, zb, eb);\n"
    "        if (eb < tol && (prm6.z > 0.5 ? zH(sb) < 0.0 : zb < body) && abs(zb - tF) < 0.03 * tF + 0.005) {\n"
    "            float3 ch = heldTex.SampleLevel(linSamp, sb, 0).rgb, cf = prm8.x > 0.5 ? freshClean.SampleLevel(linSamp, sF, 0).rgb : freshTex.SampleLevel(linSamp, sF, 0).rgb;\n"
    "            float3 dc = abs(ch - cf);\n"
    "            if (max(dc.r, max(dc.g, dc.b)) < prm6.y) return shade(heldTex, sb, 7, zb);\n"
    "        }\n"
    "    }\n"
    "    if (okF && bF) return shadeF(sF, 0, tF);\n"
    // Run 22: near the left and right edges the fresh eye's frame does not contain every part of the hands the held eye
    // should show (at hand distance the two frames are offset by about a tenth of the width), and those texels fell to
    // the world or the fill: parts of the arm invisible every other frame. Where the held eye's own hands (as fixed in
    // tracking space) land here and the fresh eye CANNOT see that point (it projects outside its frame), keep them.
    // Run 23: and, with the controllers still, anywhere near the held eye's own hands where the fresh eye sees that point
    // HIDDEN behind something nearer (a thumb behind the palm from the other eye): run 22's capture showed the thumb
    // missing at some hand angles. Near = the held image has foreground within four grid steps of the target texel.
    "    bool nearHeldFg = prm9.z > 0.5 && (isFg(zH(t)) || isFg(zH(t + float2(prm4.z * 4.0, 0))) || isFg(zH(t - float2(prm4.z * 4.0, 0)))\n"
    "                      || isFg(zH(t + float2(0, prm4.w * 4.0))) || isFg(zH(t - float2(0, prm4.w * 4.0))));\n"
    "    if (st && tp && prm9.x > 0.5 && (t.x < prm9.y || t.x > 1.0 - prm9.y || nearHeldFg)) {\n"
    "        float zb, eb; float2 sb = solveHb(t, zb, eb);\n"
    "        if (eb < tol && (prm6.z > 0.5 ? zH(sb) < 0.0 : zb < body)) {\n"
    "            float2 tn = tanFor(zH(sb));\n"
    "            float3 Wt = mc(d0, d1, d2, viewDirT(t, tn) * (zb * prm.z)) + dp.xyz;\n"
    "            float3 m = toTT(mc(f0, f1, f2, Wt - fp.xyz), tn);\n"
    "            float2 uf = ndcUV(m.xy);\n"
    "            bool outside = !(m.z > 0.0 && all(uf > 0.0) && all(uf < 1.0));\n"
    "            bool hidden = !outside && prm9.z > 0.5 && aF(uf) < m.z * (1.0 - prm3.w) - 0.005;\n"
    "            if (outside || hidden) return shade(heldTex, sb, 4, zb);\n"
    "        }\n"
    "    }\n"
    "    if (st && tp) {\n"
    // The stale test: the held point, carried to this instant as static, seen from the fresh eye.
    "        bool stale = false;\n"
    "        if (eH < prm5.w) {\n"
    "            float2 tn = tanFor(zH(sH));\n"
    "            float3 Wt = mc(d0, d1, d2, viewDirT(t, tn) * (tH * prm.z)) + dp.xyz;\n"
    "            float3 m = toTT(mc(f0, f1, f2, Wt - fp.xyz), tn);\n"
    "            float2 uf = ndcUV(m.xy);\n"
    // The NEAREST fresh depth around the projected point, 1-2 depth texels along the baseline and 1 across:
    // at a silhouette the projection lands a texel past the edge and the fresh eye sees the sky there, which
    // is parallax, not motion (run 7: the silhouette fringe). A moved object leaves the whole neighbourhood behind it.
    "            if (m.z > 0 && all(uf > 0.0) && all(uf < 1.0)) {\n"
    "                float2 o = prm4.xy * 0.5;\n"
    "                float zn = min(min(aF(uf), min(aF(uf + float2(o.x, 0)), aF(uf - float2(o.x, 0)))),\n"
    "                               min(min(aF(uf + float2(2.0 * o.x, 0)), aF(uf - float2(2.0 * o.x, 0))),\n"
    "                                   min(aF(uf + float2(0, o.y)), aF(uf - float2(0, o.y)))));\n"
    // The foreground draws on top even behind a wall. Seeing it instead of the held world
    // is occlusion, not evidence that the world moved away. Keep that eye's valid background.
    "                stale = !isFg(zF(uf)) && zn > m.z * (1.0 + prm3.w) + 0.01;\n"
    "            }\n"
    "        }\n"
    "        if (okH && !bH && !stale && !(okF && tF < tH * (1.0 - prm3.w))) return shade(heldTex, sH, 1, tH);\n"
    // A silhouette: the held eye's own sample just misses (the edge texel's depth is coarse, most of all
    // under an upscaler) while the fresh eye, 6 cm aside, sees PAST the edge to something farther. That
    // farther surface is parallax, not the answer: keep the held eye's nearer near-miss (run 7: a 1-texel
    // light fringe on roofs and trees against the sky was this).
    "        if (!okH && !stale && !bH && eH < prm5.w && okF && tF > tH * (1.0 + prm3.w)) return shade(heldTex, sH, 6, tH);\n"
    "        if (okF) return shadeF(sF, 2, tF);\n"
    "        if (okH && !stale) return shade(heldTex, sH, 3, tH);\n"
    // Neither is consistent to 1.5 texels. A thin structure (a grate slat 2-3 texels wide) often leaves
    // both a little over; the nearer miss is still the right surface, where the fill would reach past
    // the slat to the far background (the run-6 capture: the white dots on a grate were this fill).
    // World surfaces only: a near miss onto the hands at the edge of a real gap would be a ghost ring.
    "        float mF = !bF ? eF : 1e9, mH = (stale || bH) ? 1e9 : eH;\n"
    "        if (min(mF, mH) < prm5.w) return mF <= mH ? shadeF(sF, 6, tF) : shade(heldTex, sH, 6, tH);\n"
    "        return fill(t, st, tp);\n"
    "    }\n"
    "    if (tp) {\n"
    "        float zb, eb; float2 sb = solveHb(t, zb, eb);\n"
    "        if ((prm6.z > 0.5 ? zH(sb) < 0.0 : zb < body) && eb < tol) return shade(heldTex, sb, 4, zb);\n"
    // A true disocclusion in a one-source rebuild (a synthesized slot, or no fresh depth): the search's last guess is a
    // stretched streak of whatever was nearest - trails behind every edge that moves. Extend the background instead,
    // unless it is a near-miss (a thin structure).
    "        if (!okH && !(eH < prm5.w)) return fill(t, false, true);\n"
    "        return shade(heldTex, sH, okH ? 1 : 3, tH);\n"
    "    }\n"
    "    return okF ? shadeF(sF, 2, tF) : (eF < prm5.w && !bF) ? shadeF(sF, 6, tF) : fill(t, true, false);\n"
    "}\n"
    // The rebuilt colour and, in a second target when one is bound, its depth in the target view (units, 0 = none).
    "struct PSO { float4 c : SV_Target0; float z : SV_Target1; };\n"
    "PSO psmain(VSOut i) { PSO o; o.c = compose(i);\n"
    // The held eye's own UI over what came from the fresh eye: where its composed image differs from its clean
    // one, taken at the target texel itself (the UI is drawn per eye at nearly the same place each present).
    "    if (g_fromFresh && prm8.y > 0.5) {\n"
    "        float3 hc = heldTex.SampleLevel(pointSamp, i.uv, 0).rgb, hk = heldClean.SampleLevel(pointSamp, i.uv, 0).rgb;\n"
    "        float3 dd = abs(hc - hk);\n"
    "        if (max(dd.r, max(dd.g, dd.b)) > prm8.z) o.c = float4(prm3.z > 0.5 ? hc * float3(1.0, 0.5, 1.0) : hc, 1.0);\n"
    "    }\n"
    "    o.z = (g_outZ > 0.0 && g_outZ < 1e8) ? g_outZ : 60000.0; return o; }\n"
    // The depth layer: an eye's depth (units, signed: the mask is negative; 0 or >= 59999 the sky) to the
    // runtime's device depth for [near, far] metres, standard (not reversed) D3D convention.
    "cbuffer X : register(b2) { float4 xp; };\n"   // metres per unit, near, far (metres), unused
    "float psxrdepth(VSOut i) : SV_Depth {\n"
    "    float z = abs(heldTex.SampleLevel(pointSamp, i.uv, 0).r);\n"
    "    float m = (z > 0.0 && z < 59999.0) ? z * xp.x : xp.z;\n"
    "    m = clamp(m, xp.y, xp.z);\n"
    "    return saturate(xp.z / (xp.z - xp.y) * (1.0 - xp.y / m));\n"
    "}\n"
    // The seed maps: a grid over the source's depth, carried into the target and depth-tested.
    "struct MOut { float4 pos : SV_Position; float3 src : TEXCOORD0; };\n"
    "MOut vsmesh(uint id : SV_VertexID) {\n"
    "    uint cell = id / 6, k = id % 6;\n"
    "    uint gw = (uint)mp2.x;\n"
    "    uint2 c = uint2(cell % gw, cell / gw);\n"
    "    uint2 off = k == 0 ? uint2(0, 0) : k == 1 ? uint2(1, 0) : k == 2 ? uint2(0, 1) : k == 3 ? uint2(1, 0) : k == 4 ? uint2(1, 1) : uint2(0, 1);\n"
    "    float2 px = min(float2(c + off) * mp.y, mp.zw - 1.0) + 0.5;\n"
    "    float2 s = px / mp.zw;\n"
    "    float zs = mp.x < 0.5 ? zF(s) : zH(s);\n"
    "    float3 m = mp.x < 0.5 ? mapF(s, zs) : mapH(s, zs);\n"
    "    MOut o;\n"
    // Run 24: the hands and weapon are drawn ON TOP of the world (the crushed depth range), even where they are
    // geometrically behind it - a blade pushed into a wall right in front of the face. Nearest-wins let the wall's
    // seed beat the blade and the blade fell to the fill (a flicker). Foreground seeds take the near half of the
    // depth range, the world the far half, so the foreground always wins, as the game draws it.
    "    float dz = m.z / (m.z + 1.0);\n"
    "    dz = isFg(zs) ? 0.5 * dz : 0.5 + 0.5 * dz;\n"
    "    o.pos = (m.z > 0 && all(abs(m.xy) < 8.0)) ? float4(m.xy, dz, 1.0) : float4(0, 0, -1, 1);\n"
    "    o.src = float3(s, m.z);\n"
    "    return o;\n"
    "}\n"
    "float4 psmesh(MOut i) : SV_Target {\n"
    // Source texels per seed-map texel: a surface keeps about one grid's worth; a sheet across a depth
    // break covers almost no source per target texel.
    "    float2 ax = ddx(i.src.xy) * mp.zw, ay = ddy(i.src.xy) * mp.zw;\n"
    "    float area = abs(ax.x * ay.y - ax.y * ay.x);\n"
    "    float nominal = (mp.z / mp2.y) * (mp.w / mp2.z);\n"
    "    return float4(i.src.xy, i.src.z / (i.src.z + 1.0), area < nominal * mp2.w ? 0.5 : 1.0);\n"
    "}\n"
    // The depth snapshot: the scene target's alpha (linear view depth) into the eye's own R16F.
    // With the pre-foreground copy bound (t1), a texel whose depth changed after the foreground pass began was
    // drawn by it: its depth is stored NEGATIVE (the mask). Without it, plain depth.
    "float psdepth(VSOut i) : SV_Target { return heldTex.Load(int3(i.pos.xy, 0)).a; }\n"
    // Run 17: the drawn foreground mask (t1, R = 1 where a foreground draw covered the texel) signs the depth.
    "float psdepthk(VSOut i) : SV_Target {\n"
    "    float a = heldTex.Load(int3(i.pos.xy, 0)).a, k = heldDepth.Load(int3(i.pos.xy, 0)).r;\n"
    "    return (a > 0.0 && k > 0.5) ? -a : a;\n"
    "}\n"
    "float psdepthm(VSOut i) : SV_Target {\n"
    "    float a = heldTex.Load(int3(i.pos.xy, 0)).a, p = heldDepth.Load(int3(i.pos.xy, 0)).a;\n"
    "    return (a > 0.0 && abs(a - p) > 1e-4 * max(a, 1e-3)) ? -a : a;\n"
    "}\n";

struct Held {
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    uint32_t w = 0, h = 0, fmt = 0;
    uint32_t serial = 0;
    uint64_t seq = 0;                        // capture order; the fresh eye is the latest
    Pose pose{};
    bool bodyOk = false;
    float bodyYaw = 0;
    Pose targets[2]{};
    bool valid = false;
    float vp[16] = {};                       // the camera-relative world view-projection it was drawn with
    float c5[3] = {};                        // its rendered c5 (the NEGATIVE camera position, uu)
    bool vpOk = false;
    float rot[3] = {};                       // the camera rotator it was drawn with (pitch, yaw, roll, degrees)
    bool rotOk = false;
    CaptureMeta meta{};                      // the pose record's identity, writer, write time and DLSS jitter
    // Its own depth, taken from the shared ring at capture (the ring moves on; this does not).
    ID3D11Texture2D* dtex = nullptr;
    ID3D11ShaderResourceView* dsrv = nullptr;
    ID3D11RenderTargetView* drtv = nullptr;
    uint32_t dw = 0, dh = 0;
    bool depthOk = false;
    bool maskOk = false;                     // its depth carries the foreground mask (or no foreground pass was drawn)
    HandPose hands[2]{};                     // MSW: the grip poses its hands were posed from (note_hands)
    // Run 15: the game image before the mod's own layers (note_clean), swapped in from the pending copy when its
    // grab serial matches this capture's; cleanOk false = only the composed image (the pre-run-15 behaviour).
    ID3D11Texture2D* ctex = nullptr;
    ID3D11ShaderResourceView* csrv = nullptr;
    bool cleanOk = false;
};
Held g_held[2];
// Run 15: the pending clean copy (note_clean, once per present before the mod's own layers are drawn), and the switch.
struct CleanCopy { ID3D11Texture2D* tex = nullptr; ID3D11ShaderResourceView* srv = nullptr; uint32_t w = 0, h = 0, fmt = 0, serial = 0; bool full = false; };
CleanCopy g_clean;
// Run 18: the stale test's relative depth tolerance. A held point the fresh eye now sees PAST by more than this moved.
// 0.03 let an NPC walking 10-20 cm in front of a wall keep her one-tick-old pixels (a trail). `afw stale <0.005..0.1>`.
// Run 21: a still weapon keeps each eye's own shine. The held eye took the hands and weapon from the fresh eye, whose
// view-dependent highlights differ: a shiny blade swapped its reflection every frame in both eyes (the hand, matte,
// did not). While the game side reports both controllers still (note_hands_still), the held eye's own weapon is
// kept wherever the fresh eye puts the same surface at the same depth - no colour test, which a shiny blade never
// passes. Moving, the fresh eye supplies it as before (the colour-free rule would lag a moving weapon).
std::atomic<bool> g_stillShade{true}, g_handsStill{false};
// Run 22: `afw edgehands on|off` - within a quarter of the width from either edge, the held eye's own hands where the
// fresh eye's frame does not contain them.
std::atomic<bool> g_edgeHands{true};
// Run 25: `afw heldhands on|off` - the held eye's hands moved by their controllers (see warp_held).
std::atomic<bool> g_heldHandsFollow{true};
uint32_t g_heldHandsUsed = 0, g_heldHandsNoPose = 0;
uint32_t g_stillUsed = 0;
std::atomic<float> g_staleTol{0.015f};   // run 18 replay: 0.03 left a walking NPC doubled; 0.015 no worse on still captures
std::atomic<bool> g_cleanOn{true};       // `afw clean on|off`: the fresh eye's hands from its clean image, the held eye's UI kept
std::atomic<float> g_cleanUi{0.006f};    // a held texel whose composed and clean colours differ by more than this is its UI
uint32_t g_cleanTaken = 0, g_cleanMissed = 0, g_cleanUsed = 0;
uint64_t g_seq = 0;
// Bumped by any thread that changes what a held image means (warp toggled, method left); the
// render thread drops both records when it sees a new value. Records are never touched off-thread.
std::atomic<uint32_t> g_epoch{0};
uint32_t g_epochSeen = 0;

std::atomic<bool> g_on{false}, g_stereo{true}, g_debug{false}, g_matrices{true}, g_fgOn{true};
std::atomic<float> g_fgFov{0.0f};      // the game camera FOV (deg): the foreground's projection; 0 = not read yet
std::atomic<float> g_fgDepth{0.30f};   // units: nearer pixels are the foreground (run-6 capture: arms/weapon <= 0.2, world >= 0.6)
uint32_t g_fgUsed = 0;
std::atomic<float> g_nearMiss{6.0f};
std::atomic<bool> g_fgMask{true};      // `afw fgmask on|off`: the foreground from what the foreground pass drew, not depth
uint32_t g_maskFg = 0, g_maskNone = 0, g_maskMissing = 0, g_maskUsed = 0, g_maskDrawn = 0, g_maskEmpty = 0;   // g_maskDrawn: from the drawn mask (run 17)
std::atomic<float> g_ownHands{0.0f};   // `afw ownhands <0..1>`: the colour agreement for the held eye's own hands; 0 = off (default: a slowly moving
                                       // weapon can pass the colour test - the host test caught 25 px of lag - and on the run-7 captures it gained 1.64% -> 1.60%)   // `afw nearmiss <texels>`; 0 = the fill for every miss (the run-6 behaviour)
std::atomic<float> g_bodyDepth{0.40f};
std::atomic<float> g_worldScale{100.0f};
std::atomic<int> g_matrixVerdict{0};
bool g_ready = false, g_failed = false;
ID3D11VertexShader* g_vs = nullptr;
ID3D11VertexShader* g_vsMesh = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11PixelShader* g_psMesh = nullptr;
ID3D11PixelShader* g_psDepth = nullptr;
ID3D11PixelShader* g_psDepthMask = nullptr;
ID3D11PixelShader* g_psDepthK = nullptr;   // run 17: signed by the drawn mask
ID3D11Buffer* g_cb = nullptr;
ID3D11Buffer* g_cbMesh = nullptr;
ID3D11SamplerState* g_lin = nullptr;
ID3D11SamplerState* g_point = nullptr;
ID3D11RasterizerState* g_rs = nullptr;
ID3D11BlendState* g_bs = nullptr;
ID3D11DepthStencilState* g_ds = nullptr;
ID3D11DepthStencilState* g_dsTest = nullptr;
ID3D11DepthStencilState* g_dsAlways = nullptr;
// VR-39: the depth layer (XR_KHR_composition_layer_depth). The compose writes the rebuilt eye's depth (units, in
// the target view) to g_zOut while it is wanted; psxrdepth turns an eye's depth into the runtime's device depth.
ID3D11PixelShader* g_psXrDepth = nullptr;
ID3D11Buffer* g_cbXr = nullptr;
std::atomic<bool> g_xrDepthWanted{false};
ID3D11Texture2D* g_zOut = nullptr;
ID3D11RenderTargetView* g_zOutRtv = nullptr;
ID3D11ShaderResourceView* g_zOutSrv = nullptr;
uint32_t g_zOutW = 0, g_zOutH = 0;
uint64_t g_zOutSeq = 0;                  // the fresh record's seq the last compose wrote it for (0 = never)
uint32_t g_xrWrites[2] = {}, g_xrRebuilt = 0, g_xrRefused = 0;
const char* g_xrLastWhy = "";

// The seed maps (the target size divided by kSeedDiv per axis) and their shared depth buffer.
struct SeedMap { ID3D11Texture2D* tex = nullptr; ID3D11ShaderResourceView* srv = nullptr; ID3D11RenderTargetView* rtv = nullptr; };
SeedMap g_seed[2];                           // 0 fresh, 1 held
ID3D11Texture2D* g_seedDepth = nullptr;
ID3D11DepthStencilView* g_seedDsv = nullptr;
uint32_t g_seedW = 0, g_seedH = 0;
const uint32_t kGridStep = 2;                // source texels per grid cell
const uint32_t kSeedDiv = 2;                 // target texels per seed texel, per axis
const float kStretchArea = 1.0f / 9.0f;      // a triangle keeping under 1/9 of its source area per texel is a sheet

// Counters for the beat (render thread only).
uint32_t g_warpsFull = 0, g_warpsTemporal = 0, g_warpsStereo = 0;
uint32_t g_noHeld = 0, g_notFresh = 0, g_noDepth = 0, g_noRtv = 0, g_notReady = 0, g_noSeed = 0;
uint32_t g_snaps = 0, g_snapLate = 0, g_snapMiss = 0;
uint32_t g_mtxUsed = 0, g_mtxNoVp = 0, g_mtxBasis = 0, g_mtxTurn = 0, g_mtxEye = 0, g_mtxCam = 0;
float g_mtxBasisMax = 0, g_mtxTurnMax = 0, g_mtxEyeMax = 0, g_mtxCamMax = 0;
uint32_t g_camVoteGood = 0, g_camVoteFlip = 0;   // session-long: the c5 reading, voted while still
bool g_camFlipLatched = false;
std::atomic<bool> g_camVoteReset{false};
uint32_t g_camHist = 0, g_camHistN = 0;   // the last 32 still votes, 1 = flipped
double g_yawAbs = 0; float g_yawMax = 0;
uint64_t g_beatMs = 0;
float g_lastClaimDeg = 0;   // the world projection's horizontal FOV the layer claims, for the beat

// GPU time of the rebuild (both seed maps and the compose): a small ring of timestamp sets.
struct Ts { ID3D11Query* dis = nullptr; ID3D11Query* a = nullptr; ID3D11Query* b = nullptr; bool pending = false; };
const int kTs = 6;
Ts g_ts[kTs];
bool g_tsOk = false;
double g_gpuSum = 0; float g_gpuMax = 0; uint32_t g_gpuN = 0;

template <class T> void rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

DXGI_FORMAT typed(DXGI_FORMAT f) {
    switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default: return f;
    }
}

// The pipeline state these passes touch, saved and put back: the passes run inside the runtime's
// submit path, and nothing after them should inherit a warp's bindings.
struct Saved {
    ID3D11RenderTargetView* rtv = nullptr; ID3D11DepthStencilView* dsv = nullptr;
    D3D11_VIEWPORT vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]; UINT nvp = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ID3D11RasterizerState* rs = nullptr;
    ID3D11BlendState* bs = nullptr; float bf[4] = {}; UINT sm = 0;
    ID3D11DepthStencilState* ds = nullptr; UINT sref = 0;
    ID3D11InputLayout* il = nullptr; D3D11_PRIMITIVE_TOPOLOGY topo = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11VertexShader* vs = nullptr; ID3D11PixelShader* ps = nullptr;
    ID3D11Buffer* vcb[2] = {}; ID3D11Buffer* pcb[2] = {};
    ID3D11ShaderResourceView* vsrv[4] = {};
    ID3D11ShaderResourceView* srv[8] = {};
    ID3D11SamplerState* vsamp[2] = {};
    ID3D11SamplerState* samp[2] = {};
    void save(ID3D11DeviceContext* c) {
        c->OMGetRenderTargets(1, &rtv, &dsv);
        c->RSGetViewports(&nvp, vp);
        c->RSGetState(&rs);
        c->OMGetBlendState(&bs, bf, &sm);
        c->OMGetDepthStencilState(&ds, &sref);
        c->IAGetInputLayout(&il);
        c->IAGetPrimitiveTopology(&topo);
        c->VSGetShader(&vs, nullptr, nullptr);
        c->PSGetShader(&ps, nullptr, nullptr);
        c->VSGetConstantBuffers(0, 2, vcb);
        c->PSGetConstantBuffers(0, 2, pcb);
        c->VSGetShaderResources(0, 4, vsrv);
        c->PSGetShaderResources(0, 8, srv);
        c->VSGetSamplers(0, 2, vsamp);
        c->PSGetSamplers(0, 2, samp);
    }
    void restore(ID3D11DeviceContext* c) {
        c->OMSetRenderTargets(1, &rtv, dsv);
        c->RSSetViewports(nvp, vp);
        c->RSSetState(rs);
        c->OMSetBlendState(bs, bf, sm);
        c->OMSetDepthStencilState(ds, sref);
        c->IASetInputLayout(il);
        c->IASetPrimitiveTopology(topo);
        c->VSSetShader(vs, nullptr, 0);
        c->PSSetShader(ps, nullptr, 0);
        c->VSSetConstantBuffers(0, 2, vcb);
        c->PSSetConstantBuffers(0, 2, pcb);
        c->VSSetShaderResources(0, 4, vsrv);
        c->PSSetShaderResources(0, 8, srv);
        c->VSSetSamplers(0, 2, vsamp);
        c->PSSetSamplers(0, 2, samp);
        rel(rtv); rel(dsv); rel(rs); rel(bs); rel(ds); rel(il); rel(vs); rel(ps);
        for (auto*& b : vcb) rel(b);
        for (auto*& b : pcb) rel(b);
        for (auto*& s : vsrv) rel(s);
        for (auto*& s : srv) rel(s);
        for (auto*& s : vsamp) rel(s);
        for (auto*& s : samp) rel(s);
    }
};

bool compile_one(PFN_D3DCompile compile, const char* entry, const char* target, ID3DBlob** out) {
    ID3DBlob* err = nullptr;
    if (SUCCEEDED(compile(kSrc, strlen(kSrc), nullptr, nullptr, nullptr, entry, target, 0, 0, out, &err))) { rel(err); return true; }
    DVR_ERROR("afw/warp: shader %s failed: %s - the held eye stays rotation-only", entry,
              err ? (const char*)err->GetBufferPointer() : "?");
    rel(err);
    return false;
}

bool init(ID3D11Device* dev) {
    if (g_ready) return true;
    if (g_failed || !dev) return false;
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    PFN_D3DCompile compile = compiler ? (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
    if (!compile) { g_failed = true; DVR_ERROR("afw/warp: d3dcompiler_47.dll missing - the held eye stays rotation-only"); return false; }
    ID3DBlob *vsb = nullptr, *vmb = nullptr, *psb = nullptr, *pmb = nullptr, *pdb = nullptr, *pdm = nullptr, *pxd = nullptr, *pdk = nullptr;
    if (!compile_one(compile, "vsmain", "vs_4_0", &vsb) || !compile_one(compile, "vsmesh", "vs_4_0", &vmb) ||
        !compile_one(compile, "psmain", "ps_4_0", &psb) || !compile_one(compile, "psmesh", "ps_4_0", &pmb) ||
        !compile_one(compile, "psdepth", "ps_4_0", &pdb) || !compile_one(compile, "psdepthm", "ps_4_0", &pdm) ||
        !compile_one(compile, "psxrdepth", "ps_4_0", &pxd) || !compile_one(compile, "psdepthk", "ps_4_0", &pdk)) {
        g_failed = true;
        rel(vsb); rel(vmb); rel(psb); rel(pmb); rel(pdb); rel(pdm); rel(pxd); rel(pdk);
        return false;
    }
    const char* step = "shaders";
    HRESULT hr = dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &g_vs);
    if (SUCCEEDED(hr)) hr = dev->CreateVertexShader(vmb->GetBufferPointer(), vmb->GetBufferSize(), nullptr, &g_vsMesh);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &g_ps);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(pmb->GetBufferPointer(), pmb->GetBufferSize(), nullptr, &g_psMesh);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(pdb->GetBufferPointer(), pdb->GetBufferSize(), nullptr, &g_psDepth);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(pdm->GetBufferPointer(), pdm->GetBufferSize(), nullptr, &g_psDepthMask);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(pxd->GetBufferPointer(), pxd->GetBufferSize(), nullptr, &g_psXrDepth);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(pdk->GetBufferPointer(), pdk->GetBufferSize(), nullptr, &g_psDepthK);
    rel(vsb); rel(vmb); rel(psb); rel(pmb); rel(pdb); rel(pdm); rel(pxd); rel(pdk);
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 46 * 16;   // s f d y (4 each), prm prm2 prm3, hI0..2 hC, tA tB tW, mD, prm4, prm5, prm6, the hands (10), prm7, mY (3), prm8, prm9
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (SUCCEEDED(hr)) { step = "constants"; hr = dev->CreateBuffer(&bd, nullptr, &g_cb); }
    bd.ByteWidth = 2 * 16;
    if (SUCCEEDED(hr)) hr = dev->CreateBuffer(&bd, nullptr, &g_cbMesh);
    bd.ByteWidth = 16;
    if (SUCCEEDED(hr)) hr = dev->CreateBuffer(&bd, nullptr, &g_cbXr);
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (SUCCEEDED(hr)) { step = "samplers"; hr = dev->CreateSamplerState(&sd, &g_lin); }
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    if (SUCCEEDED(hr)) hr = dev->CreateSamplerState(&sd, &g_point);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    if (SUCCEEDED(hr)) { step = "raster"; hr = dev->CreateRasterizerState(&rd, &g_rs); }
    D3D11_BLEND_DESC bl = {};
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (SUCCEEDED(hr)) { step = "blend"; hr = dev->CreateBlendState(&bl, &g_bs); }
    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    if (SUCCEEDED(hr)) { step = "depth state"; hr = dev->CreateDepthStencilState(&dd, &g_ds); }
    dd.DepthEnable = TRUE; dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dd.DepthFunc = D3D11_COMPARISON_LESS;
    if (SUCCEEDED(hr)) hr = dev->CreateDepthStencilState(&dd, &g_dsTest);
    dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (SUCCEEDED(hr)) hr = dev->CreateDepthStencilState(&dd, &g_dsAlways);
    if (FAILED(hr)) {
        g_failed = true;
        DVR_ERROR("afw/warp: D3D11 objects failed at %s (0x%08lx) - the held eye stays rotation-only", step, (unsigned long)hr);
        return false;
    }
    // GPU timing is an instrument, never a condition: a refusal only blanks the ms on the beat.
    g_tsOk = true;
    D3D11_QUERY_DESC qd = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0}, qt = {D3D11_QUERY_TIMESTAMP, 0};
    for (Ts& s : g_ts)
        if (FAILED(dev->CreateQuery(&qd, &s.dis)) || FAILED(dev->CreateQuery(&qt, &s.a)) || FAILED(dev->CreateQuery(&qt, &s.b)))
            g_tsOk = false;
    g_ready = true;
    DVR_INFO("afw/warp: ready - the held eye is rebuilt each present from both images: depth-tested seed maps (a %u-texel "
             "grid of each image carried into the held eye's view), then per pixel the hands/weapon (nearer than %.2f depth "
             "units) from the FRESH eye, the world from the held eye's own image unless the fresh eye sees through it "
             "(stale), the rest from the fresh eye (GPU timing %s)", kGridStep, g_bodyDepth.load(), g_tsOk ? "on" : "unavailable");
    return true;
}

// Seed maps sized for a target of w x h.
bool ensure_seeds(ID3D11Device* dev, uint32_t w, uint32_t h) {
    const uint32_t sw = (w + kSeedDiv - 1) / kSeedDiv, sh = (h + kSeedDiv - 1) / kSeedDiv;
    if (g_seedDsv && g_seedW == sw && g_seedH == sh) return true;
    for (SeedMap& m : g_seed) { rel(m.rtv); rel(m.srv); rel(m.tex); }
    rel(g_seedDsv); rel(g_seedDepth); g_seedW = g_seedH = 0;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = sw; td.Height = sh; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    td.Format = DXGI_FORMAT_R16G16B16A16_UNORM; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = S_OK; const char* step = "seed map";
    for (SeedMap& m : g_seed) {
        if (SUCCEEDED(hr)) hr = dev->CreateTexture2D(&td, nullptr, &m.tex);
        if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(m.tex, nullptr, &m.srv);
        if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(m.tex, nullptr, &m.rtv);
    }
    td.Format = DXGI_FORMAT_D32_FLOAT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (SUCCEEDED(hr)) { step = "seed depth"; hr = dev->CreateTexture2D(&td, nullptr, &g_seedDepth); }
    if (SUCCEEDED(hr)) hr = dev->CreateDepthStencilView(g_seedDepth, nullptr, &g_seedDsv);
    if (FAILED(hr)) {
        for (SeedMap& m : g_seed) { rel(m.rtv); rel(m.srv); rel(m.tex); }
        rel(g_seedDsv); rel(g_seedDepth);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "afw/warp: seed maps %ux%u refused at %s (0x%08lx)", sw, sh, step,
                         (unsigned long)hr);
        return false;
    }
    g_seedW = sw; g_seedH = sh;
    DVR_INFO("afw/warp: seed maps %ux%u (RGBA16 UNORM x2 + D32) for a %ux%u target", sw, sh, w, h);
    return true;
}

void rows(const Pose& p, float out[3][4], bool transpose) {
    const float x = p.q[0], y = p.q[1], z = p.q[2], w = p.q[3];
    const float m[3][3] = {
        {1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
        {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
        {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) out[r][c] = transpose ? m[c][r] : m[r][c];
        out[r][3] = 0;
    }
}

void setup_draw(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv, uint32_t w, uint32_t h,
                ID3D11VertexShader* vs, ID3D11PixelShader* ps) {
    D3D11_VIEWPORT vp = {0, 0, (float)w, (float)h, 0, 1};
    ctx->OMSetRenderTargets(1, &rtv, dsv);
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(g_rs);
    const float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(g_bs, bf, 0xffffffff);
    ctx->OMSetDepthStencilState(dsv ? g_dsTest : g_ds, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ID3D11SamplerState* samps[2] = {g_lin, g_point};
    ctx->PSSetSamplers(0, 2, samps);
    ctx->VSSetSamplers(0, 2, samps);
}

// Take this eye's depth out of the shared ring into its own R16F while the ring still holds it.
bool snapshot_depth(ID3D11Device* dev, ID3D11DeviceContext* ctx, Held& h) {
    UINT dw = 0, dh = 0;
    ID3D11ShaderResourceView* src = dvr::depthprobe::depth_srv_for(h.serial, &dw, &dh);
    if (!src || !dw || !dh) { dvr::depthprobe::read_done(ctx); return false; }
    if (!h.dtex || h.dw != dw || h.dh != dh) {
        rel(h.drtv); rel(h.dsrv); rel(h.dtex);
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = dw; td.Height = dh; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R16_FLOAT;
        td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        const char* step = "texture";
        HRESULT hr = dev->CreateTexture2D(&td, nullptr, &h.dtex);
        if (SUCCEEDED(hr)) { step = "view"; hr = dev->CreateShaderResourceView(h.dtex, nullptr, &h.dsrv); }
        if (SUCCEEDED(hr)) { step = "target"; hr = dev->CreateRenderTargetView(h.dtex, nullptr, &h.drtv); }
        if (FAILED(hr)) {
            rel(h.drtv); rel(h.dsrv); rel(h.dtex); h.dw = h.dh = 0;
            dvr::depthprobe::read_done(ctx);
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "afw/warp: the per-eye depth copy %ux%u R16F was refused at %s "
                             "(0x%08lx)", dw, dh, step, (unsigned long)hr);
            return false;
        }
        h.dw = dw; h.dh = dh;
        DVR_INFO("afw/warp: per-eye depth copy %ux%u R16F (the ring moves on each present; this stays with its image)", dw, dh);
    }
    // The foreground mask: the pre-foreground copy of the same frame. No copy for this grab while the ring runs =
    // no foreground pass was drawn (nothing is foreground); a copy not yet finished = the mask is unknown.
    // Run 17: the DRAWN mask first (depth_probe's fgmask: every foreground draw drawn again into it). A mask of this
    // grab with no draws in it is a frame without a foreground pass: valid, nothing is foreground.
    uint32_t kDraws = 0, kw = 0, kh = 0;
    ID3D11ShaderResourceView* drawn = g_fgMask.load() ? dvr::depthprobe::fgmask_srv_for(h.serial, &kDraws, &kw, &kh) : nullptr;
    if (drawn && (kw != dw || kh != dh)) { drawn = nullptr; ++g_maskMissing; }
    // Run 18: an EMPTY drawn mask is not evidence of "no hands" - run 17's redraw never ran, every mask was empty, and
    // the hands became world (a constant flicker). With no draw in it the mask is unknown: the fallbacks decide.
    if (drawn && kDraws == 0) { drawn = nullptr; ++g_maskEmpty; }
    bool saw = false;
    ID3D11ShaderResourceView* pre = (g_fgMask.load() && !drawn) ? dvr::depthprobe::prefg_srv_for(h.serial, &saw) : nullptr;
    if (drawn) { h.maskOk = true; (kDraws ? g_maskFg : g_maskNone)++; ++g_maskDrawn; }
    else {
        h.maskOk = g_fgMask.load() && dvr::depthprobe::prefg_ready() && (pre || !saw);
        if (h.maskOk) (pre ? g_maskFg : g_maskNone)++; else if (g_fgMask.load()) ++g_maskMissing;
    }
    Saved sv; sv.save(ctx);
    setup_draw(ctx, h.drtv, nullptr, dw, dh, g_vs, drawn ? g_psDepthK : pre ? g_psDepthMask : g_psDepth);
    ID3D11ShaderResourceView* srvs[6] = {src, drawn ? drawn : pre, nullptr, nullptr, nullptr, nullptr};
    ctx->PSSetShaderResources(0, 6, srvs);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[8] = {};
    ctx->PSSetShaderResources(0, 8, none);
    sv.restore(ctx);
    dvr::depthprobe::read_done(ctx);
    h.depthOk = true;
    return true;
}

// ---- the game's matrices on the CPU ----------------------------------------------------------------
struct Lin { double A[4], B[4], W[4]; };
Lin lin_of(const float* vp) {   // row-vector: clip_i = sum_j P_j * M[j][i] + M[3][i]; columns 0, 1, 3
    Lin l;
    for (int j = 0; j < 4; ++j) { l.A[j] = vp[j * 4 + 0]; l.B[j] = vp[j * 4 + 1]; l.W[j] = vp[j * 4 + 3]; }
    return l;
}
void cross3(const double a[3], const double b[3], double o[3]) {
    o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0];
}
double dot3(const double a[3], const double b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
// The inverse of the clip rows [A; B; W] (xyz), as rows: P = I (nx w - A.w, ny w - B.w, w - W.w).
bool inverse_rows(const Lin& l, double I[3][3]) {
    double c0[3], c1[3], c2[3];
    cross3(l.B, l.W, c0); cross3(l.W, l.A, c1); cross3(l.A, l.B, c2);
    const double det = dot3(l.A, c0);
    if (!(fabs(det) > 1e-20)) return false;
    for (int k = 0; k < 3; ++k) { I[k][0] = c0[k] / det; I[k][1] = c1[k] / det; I[k][2] = c2[k] / det; }
    return true;
}
bool rel_from(const Lin& l, double nx, double ny, double w, double P[3]) {
    double I[3][3];
    if (!inverse_rows(l, I)) return false;
    const double r[3] = {nx * w - l.A[3], ny * w - l.B[3], w - l.W[3]};
    for (int k = 0; k < 3; ++k) P[k] = I[k][0] * r[0] + I[k][1] * r[1] + I[k][2] * r[2];
    return true;
}
bool proj(const Lin& l, const double P[3], double* nx, double* ny) {
    const double w = dot3(l.W, P) + l.W[3];
    if (!(w > 1e-3)) return false;
    *nx = (dot3(l.A, P) + l.A[3]) / w; *ny = (dot3(l.B, P) + l.B[3]) / w;
    return true;
}
// Rotate by a pose's rotation (R v) or its inverse (R^T v).
void rot(const Pose& p, const double v[3], double o[3], bool inverse) {
    float m[3][4]; rows(p, m, inverse);
    for (int r = 0; r < 3; ++r) o[r] = m[r][0] * v[0] + m[r][1] * v[1] + m[r][2] * v[2];
}
double angle_deg(double ax, double ay, double bx, double by, double tanH, double tanV) {
    const double a[3] = {ax * tanH, ay * tanV, -1}, b[3] = {bx * tanH, by * tanV, -1};
    const double c = dot3(a, b) / sqrt(dot3(a, a) * dot3(b, b));
    return acos(c > 1 ? 1 : c < -1 ? -1 : c) * 57.29577951;
}
double axis_angle(const double a[3], const float b[3]) {
    const double bd[3] = {b[0], b[1], b[2]};
    const double c = dot3(a, bd) / sqrt(dot3(a, a) * dot3(bd, bd));
    return acos(c > 1 ? 1 : c < -1 ? -1 : c) * 57.29577951;
}
// A matrix's world axes: w is the forward depth; x and y are right and up (Gram-Schmidt against forward).
bool axes_of(const Lin& l, double fwd[3], double right[3], double up[3]) {
    for (int k = 0; k < 3; ++k) { fwd[k] = l.W[k]; right[k] = l.A[k]; up[k] = l.B[k]; }
    const double fl = sqrt(dot3(fwd, fwd));
    if (!(fl > 1e-9)) return false;
    for (int k = 0; k < 3; ++k) fwd[k] /= fl;
    const double ra = dot3(right, fwd), ua = dot3(up, fwd);
    for (int k = 0; k < 3; ++k) { right[k] -= ra * fwd[k]; up[k] -= ua * fwd[k]; }
    const double rl = sqrt(dot3(right, right)), ul = sqrt(dot3(up, up));
    if (!(rl > 1e-9) || !(ul > 1e-9)) return false;
    for (int k = 0; k < 3; ++k) { right[k] /= rl; up[k] /= ul; }
    return true;
}

// The held image's world carried to the target by the GAME's matrices: the camera-relative
// view-projection each image was drawn with and its rendered camera, the pair the DLSS vectors use.
// The target camera is the fresh camera moved by the XR offset between the fresh eye and the held eye.
// Four checks, each able to refuse (prm2.w stays 0: the XR pose + body yaw world):
//   basis   each matrix's axes against the camera ROTATOR recorded with the image (an independent
//           source): a mirrored or swapped axis shows here even when both matrices share it
//   turn    far directions through the matrices against the XR poses and the body yaw
//   eye     the same instant at half a metre: the target camera's offset against the XR eye poses
//   camera  the camera displacement the c5 pair implies, less what the XR head motion explains:
//           what is left is the body's walking, bounded per present; a flipped c5 leaves about twice
//           the eye separation there
enum Verdict { kUnused = 0, kUsed = 1, kNoVp = 2, kBasis = 3, kTurn = 4, kEye = 5, kCamera = 6, kOff = 7 };
int matrix_world(const Held& src, const Held& fr, const Pose& tgt, float yawDeg, float tanH, float tanV,
                 float* hI, float* hC, float* tA, float* tB, float* tW, float* mD) {
    if (g_camVoteReset.exchange(false)) { g_camVoteGood = g_camVoteFlip = 0; g_camFlipLatched = false; g_camHist = g_camHistN = 0; }
    if (!g_matrices.load()) return kOff;
    if (!src.vpOk || !fr.vpOk || !src.rotOk || !fr.rotOk) { ++g_mtxNoVp; return kNoVp; }
    const Lin lh = lin_of(src.vp), lt = lin_of(fr.vp);
    const float scale = g_worldScale.load();
    double fwd[3], right[3], up[3], hf[3], hr[3], hu[3];
    double I[3][3];
    if (!axes_of(lt, fwd, right, up) || !axes_of(lh, hf, hr, hu) || !inverse_rows(lh, I)) { ++g_mtxNoVp; return kNoVp; }
    // Check 1, the basis: each matrix against its own rotator.
    double basisWorst = 0, ang[2][3] = {};
    for (int k = 0; k < 2; ++k) {
        const Held& e = k ? src : fr;
        const double* f = k ? hf : fwd; const double* r = k ? hr : right; const double* u = k ? hu : up;
        const dvr::clarity::Basis b = dvr::clarity::basis_from_rotator(e.rot[0], e.rot[1], e.rot[2]);
        ang[k][0] = axis_angle(f, b.f); ang[k][1] = axis_angle(r, b.r); ang[k][2] = axis_angle(u, b.u);
        basisWorst = fmax(basisWorst, fmax(ang[k][0], fmax(ang[k][1], ang[k][2])));
    }
    if (basisWorst > g_mtxBasisMax) g_mtxBasisMax = (float)basisWorst;
    // The rotator is the camera the seam WROTE, about 20 ms before the image; the rendered view carries a later
    // head yaw (run 6: forward and right 1.0 deg apart, up 0.03 - a pure yaw lag while turning). This check is for
    // conventions (a mirrored or swapped axis is 90-180 deg); the turn, eye and camera checks hold the numbers.
    // 75 deg, not 10: the check exists for conventions (a mirrored or swapped axis is 90-180 deg). The rotator is written
    // 20-30 ms before the image, and in a fast stick turn (run 11: 500-600 deg/s) that lag alone reads 10-23 deg - a
    // snap turn more - and each refusal dropped the held eye's world to the XR model for a present: trails while turning.
    if (basisWorst > 75.0) {
        ++g_mtxBasis;
        // Name the disagreement: which record, which axis, the rotator against the angles the matrix implies
        // (UE: yaw from forward x/y, pitch from forward z, roll from right z), and whose camera it was.
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
            "afw/warp: basis refused %.2f deg | fresh rec %u writer %d written %.1f ms before its capture: fwd %.2f right %.2f "
            "up %.2f deg, rotator p/y/r %.2f/%.2f/%.2f, matrix p/y/r %.2f/%.2f/%.2f | held rec %u writer %d: fwd %.2f right %.2f "
            "up %.2f deg, rotator p/y/r %.2f/%.2f/%.2f, matrix p/y/r %.2f/%.2f/%.2f",
            basisWorst, fr.meta.recId, fr.meta.writer, fr.meta.captureMs - fr.meta.writeMs, ang[0][0], ang[0][1], ang[0][2],
            fr.rot[0], fr.rot[1], fr.rot[2], asin(fwd[2]) * 57.29578, atan2(fwd[1], fwd[0]) * 57.29578,
            asin(-right[2]) * 57.29578, src.meta.recId, src.meta.writer, ang[1][0], ang[1][1], ang[1][2],
            src.rot[0], src.rot[1], src.rot[2], asin(hf[2]) * 57.29578, atan2(hf[1], hf[0]) * 57.29578,
            asin(-hr[2]) * 57.29578);
        return kBasis;
    }
    // The eye offset: tracking (fresh eye -> held eye) into the fresh XR view, then into the game world.
    auto to_world = [&](const double tr[3], double out[3]) {
        double ev[3]; rot(fr.pose, tr, ev, true);
        for (int k = 0; k < 3; ++k) out[k] = scale * (-ev[2] * fwd[k] + ev[0] * right[k] + ev[1] * up[k]);
    };
    const double de[3] = {tgt.p[0] - fr.pose.p[0], tgt.p[1] - fr.pose.p[1], tgt.p[2] - fr.pose.p[2]};
    double O[3]; to_world(de, O);
    // Check 2, the turn: far directions through both models.
    const double pts[5][2] = {{0, 0}, {0.5, 0}, {-0.5, 0}, {0, 0.5}, {0, -0.5}};
    const float a = yawDeg / 57.29578f, ca = cosf(a), sa = sinf(a);
    double turnWorst = 0, eyeWorst = 0;
    for (const auto& pt : pts) {
        const double v[3] = {pt[0] * tanH, pt[1] * tanV, -1};
        double dT[3]; rot(tgt, v, dT, false);
        const double dW[3] = {ca * dT[0] - sa * dT[2], dT[1], sa * dT[0] + ca * dT[2]};   // Ry^T
        double dS[3]; rot(src.pose, dW, dS, true);
        if (!(dS[2] < -1e-6)) { turnWorst = 180; break; }
        double P[3], mx, my;
        if (!rel_from(lt, pt[0], pt[1], 1.0e7, P) || !proj(lh, P, &mx, &my)) { turnWorst = 180; break; }
        turnWorst = fmax(turnWorst, angle_deg(dS[0] / -dS[2] / tanH, dS[1] / -dS[2] / tanV, mx, my, tanH, tanV));
    }
    if (turnWorst > g_mtxTurnMax) g_mtxTurnMax = (float)turnWorst;
    // 5 deg, not 0.5: the recorded body yaw lags the rendered view by up to 3 deg in a fast stick turn (run 7),
    // and each refusal drops the held eye to the XR model for one present - a jump while turning. Axis and
    // convention mistakes (90-180 deg) are the basis check's.
    // The body yaw it compares against is recorded with the rotator, 20-30 ms before the image: in a turn it lags by about
    // twice the per-present yaw (run 11: 5.7-8.1 deg refusals at 4.8-5.7 deg per present). The limit grows with the turn.
    const double turnLimit = 5.0 + 2.0 * fabs((double)yawDeg);
    if (turnWorst > turnLimit) { ++g_mtxTurn; return kTurn; }
    // Check 3, the eye: the same instant, half a metre out, the target seen from the fresh eye.
    for (const auto& pt : pts) {
        const double v[3] = {pt[0] * tanH * 0.5, pt[1] * tanV * 0.5, -0.5};
        double wt[3]; rot(tgt, v, wt, false);
        const double rl[3] = {wt[0] + tgt.p[0] - fr.pose.p[0], wt[1] + tgt.p[1] - fr.pose.p[1], wt[2] + tgt.p[2] - fr.pose.p[2]};
        double lf[3]; rot(fr.pose, rl, lf, true);
        if (!(lf[2] < -1e-6)) { eyeWorst = 180; break; }
        double P[3], mx, my;
        if (!rel_from(lt, pt[0], pt[1], 0.5 * scale, P)) { eyeWorst = 180; break; }
        for (int k = 0; k < 3; ++k) P[k] += O[k];
        if (!proj(lt, P, &mx, &my)) { eyeWorst = 180; break; }
        eyeWorst = fmax(eyeWorst, angle_deg(lf[0] / -lf[2] / tanH, lf[1] / -lf[2] / tanV, mx, my, tanH, tanV));
    }
    if (eyeWorst > g_mtxEyeMax) g_mtxEyeMax = (float)eyeWorst;
    if (eyeWorst > 0.5) { ++g_mtxEye; return kEye; }
    // Check 4, the camera: D = held camera - target camera, against the XR head motion held -> target.
    double D[3];
    for (int k = 0; k < 3; ++k) D[k] = (-src.c5[k]) - ((-fr.c5[k]) + O[k]);
    const double dh[3] = {src.pose.p[0] - tgt.p[0], src.pose.p[1] - tgt.p[1], src.pose.p[2] - tgt.p[2]};
    double Dx[3]; to_world(dh, Dx);
    const double res[3] = {D[0] - Dx[0], D[1] - Dx[1], D[2] - Dx[2]};
    const double camResid = sqrt(dot3(res, res));
    // What is left is the body's own motion, which running makes large (run 7: 8-28 uu in a present); a fixed
    // bound refused the matrices exactly while running and the held eye's world then lagged the run. The check
    // exists for a flipped c5, which would be a permanent fault, not a per-present one - so it is a VOTE over
    // presents where the body is nearly still (either reading under 8 uu): a flip leaves about twice the eye
    // separation, so there the as-is reading must be clearly the better one. Per present only a jump past 150 uu
    // (a Blink, a teleport) refuses.
    double Df[3];
    for (int k = 0; k < 3; ++k) Df[k] = src.c5[k] - (fr.c5[k] + O[k]);
    const double rf[3] = {Df[0] - Dx[0], Df[1] - Dx[1], Df[2] - Dx[2]};
    const double flipResid = sqrt(dot3(rf, rf));
    if (camResid > g_mtxCamMax) g_mtxCamMax = (float)camResid;
    if (fmin(camResid, flipResid) < 8.0) {
        // The last 32 still votes: 28 of them for a flip latches (a session-long count let a long correct history
        // outvote a fault that began later).
        if (camResid < 0.5 * flipResid) { ++g_camVoteGood; g_camHist <<= 1; if (g_camHistN < 32) ++g_camHistN; }
        else if (flipResid < 0.5 * camResid) { ++g_camVoteFlip; g_camHist = (g_camHist << 1) | 1u; if (g_camHistN < 32) ++g_camHistN; }
        int flips = 0; for (uint32_t m = g_camHist; m; m &= m - 1) ++flips;
        if (!g_camFlipLatched && g_camHistN >= 32 && flips >= 28) {
            g_camFlipLatched = true;
            DVR_WARN("afw/warp: the camera check says c5 reads FLIPPED (%u presents for, %u against, while still) - the "
                     "matrices are refused for the session; walking in the held eye lags a tick", g_camVoteFlip, g_camVoteGood);
        }
    }
    if (camResid > 150.0 || g_camFlipLatched) { ++g_mtxCam; return kCamera; }
    for (int k = 0; k < 3; ++k) {
        for (int j = 0; j < 3; ++j) hI[k * 4 + j] = (float)I[k][j];
        hI[k * 4 + 3] = 0;
    }
    hC[0] = (float)lh.A[3]; hC[1] = (float)lh.B[3]; hC[2] = (float)lh.W[3]; hC[3] = 0;
    for (int j = 0; j < 4; ++j) { tA[j] = (float)lt.A[j]; tB[j] = (float)lt.B[j]; tW[j] = (float)lt.W[j]; }
    for (int k = 0; k < 3; ++k) mD[k] = (float)D[k];
    mD[3] = 0;
    ++g_mtxUsed;
    return kUsed;
}

void poll_timestamps(ID3D11DeviceContext* ctx) {
    if (!g_tsOk) return;
    for (Ts& s : g_ts) {
        if (!s.pending) continue;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj = {};
        UINT64 a = 0, b = 0;
        if (ctx->GetData(s.dis, &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
        if (ctx->GetData(s.a, &a, sizeof(a), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
        if (ctx->GetData(s.b, &b, sizeof(b), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
        s.pending = false;
        if (dj.Disjoint || !dj.Frequency || b < a) continue;
        const float ms = (float)((double)(b - a) * 1000.0 / (double)dj.Frequency);
        g_gpuSum += ms; ++g_gpuN;
        if (ms > g_gpuMax) g_gpuMax = ms;
    }
}

void beat() {
    const uint64_t now = GetTickCount64();
    if (!g_beatMs) { g_beatMs = now; return; }
    if (now - g_beatMs < 3000) return;
    g_beatMs = now;
    const uint32_t warps = g_warpsFull + g_warpsTemporal + g_warpsStereo;
    if (warps + g_noHeld + g_notFresh + g_noDepth + g_noRtv + g_notReady + g_noSeed) {
        char gpu[64] = "n/a";
        if (g_gpuN) _snprintf_s(gpu, sizeof(gpu), _TRUNCATE, "%.3f ms mean, %.3f max", g_gpuSum / g_gpuN, g_gpuMax);
        DVR_INFO("afw/warp: beat %u held-eye rebuilds - full %u (hands from the fresh eye, world from the held eye), "
                 "temporal only %u (no fresh depth: the hands carried by the body hypothesis, they can ghost), fresh only "
                 "%u (no held image or depth) | mean |yaw| %.3f deg, max %.3f | NOT rebuilt (each falls back to the "
                 "rotation-only held eye, a visible pop): no fresh image %u, fresh eye not captured this present %u, no depth "
                 "%u, no render target %u, no seed maps %u, not ready %u | depth copies %u (%u taken late at the warp, %u "
                 "missed: the ring had moved on or its copy was not finished) | world by the GAME matrices %u (walking "
                 "carried), by the XR pose + body yaw otherwise: no matrices %u, refused by basis %u (worst %.2f deg), turn "
                 "%u (%.2f deg), eye %u (%.2f deg), camera %u (worst residual %.1f uu) - each check compares two sources that "
                 "must agree | foreground MASK on %u rebuilds (images with a foreground pass %u, without %u, mask unknown %u: "
                 "those fall back to the depth limit) | foreground projection %.2f deg below %.2f units on %u (the world's %.2f deg elsewhere; 0 = off) "
                 "| GPU %s | body < %.2f units, %.3f m per unit%s",
                 warps, g_warpsFull, g_warpsTemporal, g_warpsStereo, warps ? g_yawAbs / warps : 0.0, g_yawMax,
                 g_noHeld, g_notFresh, g_noDepth, g_noRtv, g_noSeed, g_notReady, g_snaps, g_snapLate, g_snapMiss,
                 g_mtxUsed, g_mtxNoVp, g_mtxBasis, g_mtxBasisMax, g_mtxTurn, g_mtxTurnMax, g_mtxEye, g_mtxEyeMax,
                 g_mtxCam, g_mtxCamMax, g_maskUsed, g_maskFg, g_maskNone, g_maskMissing, g_fgOn.load() ? g_fgFov.load() : 0.0f, g_fgDepth.load(), g_fgUsed, g_lastClaimDeg,
                 gpu, g_bodyDepth.load(), dvr::clarity::depth_scale() / g_worldScale.load(),
                 g_stereo.load() ? "" : " | fresh-eye source OFF (afw stereo off)");
    }
    // Run 17: which classification the hands and weapon got (the beat line above is cut in the log before its end).
    if (g_maskDrawn + g_maskMissing + g_maskFg + g_maskNone + g_maskEmpty)
        DVR_INFO("afw/warp: foreground from the DRAWN mask on %u images, %u drawn masks EMPTY (not trusted: the depth "
                 "limit decides there); signed images %u, unsigned %u, mask unknown %u (the depth limit, %.2f units: a close "
                 "face becomes hands, a far blade becomes world); rebuilds with the mask on %u",
                 g_maskDrawn, g_maskEmpty, g_maskFg, g_maskNone, g_maskMissing, g_fgDepth.load(), g_maskUsed);
    g_maskDrawn = g_maskEmpty = 0;
    if (g_stillUsed || g_stillShade.load())
        DVR_INFO("afw/warp: still-weapon shading %s - %u rebuilds kept the held eye's own weapon shading (both controllers "
                 "still); controllers %s now", g_stillShade.load() ? "ON" : "off", g_stillUsed,
                 g_handsStill.load() ? "STILL" : "moving");
    g_stillUsed = 0;
    if (g_heldHandsUsed + g_heldHandsNoPose)
        DVR_INFO("afw/warp: held hands %s - %u rebuilds moved the held eye's hands by their controllers, %u had no grip pose "
                 "for both images (their held hands stay where the image drew them)", g_heldHandsFollow.load() ? "FOLLOW" : "off",
                 g_heldHandsUsed, g_heldHandsNoPose);
    g_heldHandsUsed = g_heldHandsNoPose = 0;
    if (g_cleanTaken + g_cleanMissed + g_cleanUsed)
        DVR_INFO("afw/warp: clean sources %s - %u captures took their clean game image, %u did not (no pending copy for "
                 "that grab: the method did not provide one, or its serial or size differed), %u rebuilds took the fresh "
                 "eye's hands from it (the rest from the composed image, UI included); held UI threshold %.3f",
                 g_cleanOn.load() ? "ON" : "off", g_cleanTaken, g_cleanMissed, g_cleanUsed, g_cleanUi.load());
    g_cleanTaken = g_cleanMissed = g_cleanUsed = 0;
    g_warpsFull = g_warpsTemporal = g_warpsStereo = 0;
    g_noHeld = g_notFresh = g_noDepth = g_noRtv = g_notReady = g_noSeed = 0;
    g_snaps = g_snapLate = g_snapMiss = 0;
    g_mtxUsed = g_mtxNoVp = g_mtxBasis = g_mtxTurn = g_mtxEye = g_mtxCam = 0;
    g_mtxBasisMax = g_mtxTurnMax = g_mtxEyeMax = g_mtxCamMax = 0;
    g_fgUsed = 0;
    g_maskFg = g_maskNone = g_maskMissing = g_maskUsed = 0;
    g_yawAbs = 0; g_yawMax = 0;
    g_gpuSum = 0; g_gpuMax = 0; g_gpuN = 0;
}

// RENDER thread: drop both records when the meaning of a held image changed since they were taken.
void sync_epoch() {
    const uint32_t e = g_epoch.load();
    if (e == g_epochSeen) return;
    g_epochSeen = e;
    for (Held& h : g_held) { h.valid = false; h.depthOk = false; }
}

} // namespace

void set_enabled(bool on, const char* who) {
    g_epoch.fetch_add(1);   // the records stop meaning anything either way
    dvr::depthprobe::set_prefg_wanted(1, on && g_fgMask.load());
    if (g_on.exchange(on) == on) return;
    DVR_INFO("afw/warp: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? "" : " - the held eye is the compositor's rotation-only reprojection (plus the body-yaw pose)");
}
bool enabled() { return g_on.load(); }
void set_stereo(bool on, const char* who) {
    g_stereo.store(on);
    DVR_INFO("afw/warp: fresh-eye source %s (%s)%s", on ? "ON" : "OFF", who ? who : "?",
             on ? " - the hands come from the fresh eye at this instant" : " - the held eye alone, hands by the body "
             "hypothesis (a moving hand ghosts, a turn leaves a trail)");
}
bool stereo() { return g_stereo.load(); }
void set_debug(bool on, const char* who) {
    g_debug.store(on);
    DVR_INFO("afw/warp: debug tint %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - in the HELD eye only: green = hands from the fresh eye, untinted = the held eye's own world, "
                  "blue = world from the fresh eye (moved or uncovered), red = last resort, yellow = temporal body hypothesis" : "");
}
bool debug() { return g_debug.load(); }
void set_matrices(bool on, const char* who) {
    if (g_matrices.exchange(on) != on) g_camVoteReset.store(true);   // a fresh vote, taken on the render thread
    DVR_INFO("afw/warp: world by the game's matrices %s (%s)%s", on ? "ON" : "OFF", who ? who : "?",
             on ? " - head, stick yaw and walking, checked each present (basis, turn, eye, camera)"
                : " - the XR pose and the body yaw alone: the held eye's world lags a tick of walking");
}
bool matrices() { return g_matrices.load(); }
void set_fg_fov(float deg) {
    const float was = g_fgFov.exchange(deg);
    if (fabsf(was - deg) > 0.05f)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000, "afw/warp: foreground projection %.2f deg (the game camera "
                         "FOV the arms and weapon are drawn with; was %.2f)", deg, was);
}
void set_fg(bool on, const char* who) {
    g_fgOn.store(on);
    DVR_INFO("afw/warp: foreground projection %s (%s)%s", on ? "ON" : "OFF", who ? who : "?",
             on ? " - arms and weapon reprojected with the game camera's FOV" : " - arms and weapon reprojected with the "
             "world's FOV (their stereo is then off by the ratio of the two)");
}
bool fg() { return g_fgOn.load(); }
void set_near_miss(float texels, const char* who) {
    if (!(texels >= 0.0f && texels <= 64.0f)) { DVR_WARN("afw/warp: near-miss %.2f refused (0..64 texels)", texels); return; }
    g_nearMiss.store(texels);
    DVR_INFO("afw/warp: near-miss %.1f texels (%s)%s", texels, who ? who : "?", texels > 0 ? "" : " - every miss goes to the fill");
}
float near_miss() { return g_nearMiss.load(); }
void set_fg_mask(bool on, const char* who) {
    g_fgMask.store(on);
    dvr::depthprobe::set_prefg_wanted(1, on && g_on.load());
    DVR_INFO("afw/warp: foreground mask %s (%s)%s", on ? "ON" : "off", who ? who : "?", on ?
             " - the arms and weapon are what the foreground pass drew (a copy of the scene target when it begins)" :
             " - the arms and weapon are whatever is nearer than the foreground depth limit");
}
bool fg_mask() { return g_fgMask.load(); }
void set_own_hands(float limit, const char* who) {
    if (!(limit >= 0.0f && limit <= 1.0f)) { DVR_WARN("afw/warp: own hands %.3f refused (0..1)", limit); return; }
    g_ownHands.store(limit);
    DVR_INFO("afw/warp: own hands %s (%s)%s", limit > 0 ? "ON" : "off", who ? who : "?", limit > 0 ?
             " - a still hand and weapon keep the held eye's own shading" : " - the hands always come from the fresh eye");
}
float own_hands() { return g_ownHands.load(); }
void set_fg_depth(float units, const char* who) {
    if (!(units >= 0.0f && units < 2.0f)) { DVR_WARN("afw/warp: foreground depth %.3f refused (0..2 units)", units); return; }
    g_fgDepth.store(units);
    DVR_INFO("afw/warp: foreground depth %.2f units (%s)", units, who ? who : "?");
}
int matrix_verdict() { return g_matrixVerdict.load(); }
void set_body_depth(float units, const char* who) {
    if (!(units > 0.0f && units < 5.0f)) { DVR_WARN("afw/warp: body depth %.3f refused (0..5 units)", units); return; }
    g_bodyDepth.store(units);
    DVR_INFO("afw/warp: body depth %.2f units (%s) - nearer pixels are the hands and weapon", units, who ? who : "?");
}
float body_depth() { return g_bodyDepth.load(); }
void set_world_scale(float uuPerM) { if (uuPerM >= 1.0f && uuPerM <= 400.0f) g_worldScale.store(uuPerM); }

bool clean_wanted() { return g_on.load() && g_cleanOn.load(); }
void note_clean(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* frame, uint32_t grabSerial) {
    if (!clean_wanted() || !dev || !ctx || !frame) return;
    D3D11_TEXTURE2D_DESC fd;
    frame->GetDesc(&fd);
    if (!g_clean.tex || g_clean.w != fd.Width || g_clean.h != fd.Height || g_clean.fmt != (uint32_t)fd.Format) {
        rel(g_clean.srv); rel(g_clean.tex);
        D3D11_TEXTURE2D_DESC td = fd;
        td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1; td.SampleDesc.Quality = 0;
        td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = 0; td.MiscFlags = 0;
        HRESULT hr = dev->CreateTexture2D(&td, nullptr, &g_clean.tex);
        D3D11_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = typed(fd.Format);
        sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sv.Texture2D.MipLevels = 1;
        if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(g_clean.tex, &sv, &g_clean.srv);
        if (FAILED(hr)) {
            rel(g_clean.srv); rel(g_clean.tex); g_clean = CleanCopy{};
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "afw/warp: clean image copy %ux%u fmt %u refused (0x%08lx) - "
                             "the hands come from the composed image (UI included)", fd.Width, fd.Height, (unsigned)fd.Format, (unsigned long)hr);
            return;
        }
        g_clean.w = fd.Width; g_clean.h = fd.Height; g_clean.fmt = (uint32_t)fd.Format;
    }
    ctx->CopyResource(g_clean.tex, frame);
    g_clean.serial = grabSerial; g_clean.full = true;
}
void set_clean(bool on, const char* who) {
    if (g_cleanOn.exchange(on) != on)
        DVR_INFO("afw/warp: clean sources %s (%s)%s", on ? "ON" : "off", who ? who : "?",
                 on ? " - the fresh eye's hands from its game image before our layers; the held eye's own UI kept over them"
                    : " - the composed images: the fresh eye's markers and F10 panel ride its hands into the held eye");
}
bool clean_on() { return g_cleanOn.load(); }
void note_hands_still(bool still) { g_handsStill.store(still); }
void set_still_shade(bool on, const char* who) {
    if (g_stillShade.exchange(on) != on)
        DVR_INFO("afw/warp: still-weapon shading %s (%s)%s", on ? "ON" : "off", who ? who : "?",
                 on ? " - with both controllers still, each eye keeps its own shine on the weapon"
                    : " - the other eye's shine on the rebuilt weapon (a shiny blade flickers)");
}
bool still_shade() { return g_stillShade.load(); }
void set_edge_hands(bool on, const char* who) {
    if (g_edgeHands.exchange(on) != on)
        DVR_INFO("afw/warp: edge hands %s (%s)%s", on ? "ON" : "off", who ? who : "?",
                 on ? " - near the frame's edges the held eye keeps its own hands where the other eye cannot see them"
                    : " - parts of the arms at the frame's edges can vanish every other frame");
}
bool edge_hands() { return g_edgeHands.load(); }
void set_held_hands(bool on, const char* who) {
    if (g_heldHandsFollow.exchange(on) != on)
        DVR_INFO("afw/warp: held hands %s (%s)%s", on ? "FOLLOW their controllers" : "off", who ? who : "?",
                 on ? " - the held eye's own hands move with the grips to now, so they can fill what the other eye cannot see"
                    : " - the held eye's own hands stay where its image drew them (used only while the controllers are still)");
}
bool held_hands() { return g_heldHandsFollow.load(); }
void set_stale(float rel, const char* who) {
    if (!(rel >= 0.005f && rel <= 0.1f)) return;
    g_staleTol.store(rel);
    DVR_INFO("afw/warp: stale tolerance %.3f (%s) - a held-eye point the fresh eye now sees more than %.1f%% past was moved",
             rel, who ? who : "?", rel * 100.0f);
}
float stale() { return g_staleTol.load(); }

void note_capture(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* frame,
                  uint32_t grabSerial, const Pose& pose, bool bodyOk, float bodyYawDeg, const Pose targets[2],
                  const float* vp16, const float* c5, const float* rotator, const CaptureMeta* meta) {
    if (!g_on.load() || eye < 0 || eye > 1 || !dev || !ctx || !frame) return;
    sync_epoch();
    if (!init(dev)) return;
    Held& h = g_held[eye];
    h.valid = false;   // until this capture is whole
    D3D11_TEXTURE2D_DESC fd;
    frame->GetDesc(&fd);
    if (!h.tex || h.w != fd.Width || h.h != fd.Height || h.fmt != (uint32_t)fd.Format) {
        rel(h.srv); rel(h.tex);
        D3D11_TEXTURE2D_DESC td = fd;
        td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1; td.SampleDesc.Quality = 0;
        td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = 0; td.MiscFlags = 0;
        HRESULT hr = dev->CreateTexture2D(&td, nullptr, &h.tex);
        D3D11_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = typed(fd.Format);
        sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sv.Texture2D.MipLevels = 1;
        if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(h.tex, &sv, &h.srv);
        if (FAILED(hr)) {
            rel(h.srv); rel(h.tex);
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "afw/warp: eye %c image copy %ux%u fmt %u refused (0x%08lx)",
                             eye ? 'R' : 'L', fd.Width, fd.Height, (unsigned)fd.Format, (unsigned long)hr);
            return;
        }
        h.w = fd.Width; h.h = fd.Height; h.fmt = (uint32_t)fd.Format;
        DVR_INFO("afw/warp: eye %c image copy %ux%u fmt %u", eye ? 'R' : 'L', h.w, h.h, h.fmt);
    }
    ctx->CopyResource(h.tex, frame);
    // Run 15: this grab's clean image, by swapping textures with the pending copy (no second copy).
    h.cleanOk = false;
    if (g_cleanOn.load() && g_clean.full && g_clean.serial == grabSerial && g_clean.w == fd.Width && g_clean.h == fd.Height &&
        g_clean.fmt == (uint32_t)fd.Format) {
        ID3D11Texture2D* t = h.ctex; ID3D11ShaderResourceView* v = h.csrv;
        h.ctex = g_clean.tex; h.csrv = g_clean.srv; g_clean.tex = t; g_clean.srv = v;
        // Run 26: the pending slot now holds whatever this record held before - after a render-size change (DLSS on/off),
        // a texture of the OLD size. Its size is read from the texture itself: keeping the pending slot's old numbers made
        // note_clean copy into a wrong-size texture (CopyResource refuses silently), a stale image went round the three
        // textures for the rest of the session, and the held-UI rule pasted it over the rebuild (a doubled sword, white
        // dots, flicker, until restart).
        g_clean.w = g_clean.h = g_clean.fmt = 0;
        if (g_clean.tex) {
            D3D11_TEXTURE2D_DESC cd;
            g_clean.tex->GetDesc(&cd);
            g_clean.w = cd.Width; g_clean.h = cd.Height; g_clean.fmt = (uint32_t)cd.Format;
        }
        g_clean.full = false;
        h.cleanOk = true; ++g_cleanTaken;
    } else if (g_cleanOn.load()) ++g_cleanMissed;
    h.serial = grabSerial; h.pose = pose; h.bodyOk = bodyOk; h.bodyYaw = bodyYawDeg;
    h.targets[0] = targets[0]; h.targets[1] = targets[1];
    h.vpOk = vp16 && c5;
    if (h.vpOk) { memcpy(h.vp, vp16, sizeof(h.vp)); memcpy(h.c5, c5, sizeof(h.c5)); }
    h.rotOk = rotator != nullptr;
    if (h.rotOk) memcpy(h.rot, rotator, sizeof(h.rot));
    h.meta = meta ? *meta : CaptureMeta{};
    h.seq = ++g_seq;
    h.depthOk = false;
    h.valid = true;
    if (snapshot_depth(dev, ctx, h)) ++g_snaps;   // a miss is retried at the warp, this same present
}

// ---- the diagnostic capture (`afw dump`, F10): a bounded run of consecutive presents -----------------
// Per present: the fresh eye's image and depth (the native frame), the held eye's image and depth, the
// rebuilt held eye, and both records. Raw rows, tightly packed, with a text header per present. Written
// to the data dir's dumps\ only on request (game output: never committed). Each present stalls the
// render thread on the readbacks - the capture is announced and bounded.
namespace {
std::atomic<int> g_dumpLeft{0};
std::atomic<uint64_t> g_dumpAtMs{0};
char g_dumpDir[MAX_PATH] = "";
uint32_t g_dumpIndex = 0;
char g_dumpStatus[160] = "no capture yet";

uint32_t bytes_per_pixel(DXGI_FORMAT f) {
    switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_R10G10B10A2_TYPELESS: case DXGI_FORMAT_R10G10B10A2_UNORM: return 4;
        case DXGI_FORMAT_R16_FLOAT: return 2;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
        case DXGI_FORMAT_R32G32B32A32_TYPELESS: case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
        default: return 0;
    }
}
bool dump_texture(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* t, const char* path, FILE* meta, const char* key) {
    if (!t) { fprintf(meta, "%s=absent\n", key); return false; }
    D3D11_TEXTURE2D_DESC d; t->GetDesc(&d);
    const uint32_t bpp = bytes_per_pixel(d.Format);
    if (!bpp || d.SampleDesc.Count != 1) { fprintf(meta, "%s=unsupported fmt %u samples %u\n", key, (unsigned)d.Format, d.SampleDesc.Count); return false; }
    D3D11_TEXTURE2D_DESC sd = d;
    sd.MipLevels = 1; sd.ArraySize = 1; sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
    ID3D11Texture2D* st = nullptr;
    if (FAILED(dev->CreateTexture2D(&sd, nullptr, &st)) || !st) { fprintf(meta, "%s=staging refused\n", key); return false; }
    ctx->CopySubresourceRegion(st, 0, 0, 0, 0, t, 0, nullptr);
    D3D11_MAPPED_SUBRESOURCE m = {};
    bool ok = false;
    if (SUCCEEDED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
        FILE* f = nullptr;
        if (!fopen_s(&f, path, "wb") && f) {
            for (uint32_t y = 0; y < d.Height; ++y) fwrite((const uint8_t*)m.pData + (size_t)y * m.RowPitch, bpp, d.Width, f);
            fclose(f); ok = true;
        }
        ctx->Unmap(st, 0);
    }
    st->Release();
    fprintf(meta, "%s=%s %ux%u fmt %u bpp %u\n", key, ok ? "ok" : "write failed", d.Width, d.Height, (unsigned)d.Format, bpp);
    return ok;
}
void meta_pose(FILE* f, const char* key, const Pose& p) {
    fprintf(f, "%s=%.7f %.7f %.7f %.7f | %.6f %.6f %.6f\n", key, p.q[0], p.q[1], p.q[2], p.q[3], p.p[0], p.p[1], p.p[2]);
}
void meta_record(FILE* f, const char* who, const Held& e) {
    fprintf(f, "%s.serial=%u\n%s.seq=%llu\n%s.rec=%u\n%s.writer=%d\n%s.writeMs=%.3f\n%s.captureMs=%.3f\n", who, e.serial, who,
            (unsigned long long)e.seq, who, e.meta.recId, who, e.meta.writer, who, e.meta.writeMs, who, e.meta.captureMs);
    fprintf(f, "%s.displayTime=%lld\n", who, (long long)e.meta.displayTime);
    fprintf(f, "%s.jitter=%.5f %.5f draws %u\n%s.bodyOk=%d\n%s.bodyYaw=%.5f\n", who, e.meta.jitter[0], e.meta.jitter[1],
            e.meta.jitterDraws, who, e.bodyOk ? 1 : 0, who, e.bodyYaw);
    char k[48];
    _snprintf_s(k, sizeof(k), _TRUNCATE, "%s.pose", who); meta_pose(f, k, e.pose);
    _snprintf_s(k, sizeof(k), _TRUNCATE, "%s.target0", who); meta_pose(f, k, e.targets[0]);
    _snprintf_s(k, sizeof(k), _TRUNCATE, "%s.target1", who); meta_pose(f, k, e.targets[1]);
    fprintf(f, "%s.vpOk=%d\n%s.vp=", who, e.vpOk ? 1 : 0, who);
    for (int i = 0; i < 16; ++i) fprintf(f, "%.9g%s", e.vp[i], i < 15 ? " " : "\n");
    fprintf(f, "%s.c5=%.4f %.4f %.4f\n%s.rotOk=%d\n%s.rot=%.5f %.5f %.5f\n", who, e.c5[0], e.c5[1], e.c5[2], who, e.rotOk ? 1 : 0,
            who, e.rot[0], e.rot[1], e.rot[2]);
    for (int k = 0; k < 2; ++k)   // run 25: the grips the image was drawn with (ok px py pz qx qy qz qw), for the replay
        fprintf(f, "%s.hand%d=%d %.6f %.6f %.6f %.7f %.7f %.7f %.7f\n", who, k, e.hands[k].ok ? 1 : 0, e.hands[k].p[0], e.hands[k].p[1],
                e.hands[k].p[2], e.hands[k].q[0], e.hands[k].q[1], e.hands[k].q[2], e.hands[k].q[3]);
}
void dump_tick(ID3D11Device* dev, ID3D11DeviceContext* ctx, const Held& src, const Held& fr, bool haveH, int held,
               ID3D11Texture2D* dst, uint32_t w, uint32_t h, float tanH, float tanV, float yawDeg, bool useS, bool useT,
               int verdict, const Pose& tgt) {
    if (g_dumpLeft.load() <= 0 || GetTickCount64() < g_dumpAtMs.load() || !g_dumpDir[0]) return;
    const uint32_t n = g_dumpIndex++;
    char base[MAX_PATH], path[MAX_PATH];
    _snprintf_s(base, sizeof(base), _TRUNCATE, "%s\\p%02u", g_dumpDir, n);
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s.txt", base);
    FILE* meta = nullptr;
    if (fopen_s(&meta, path, "w") || !meta) { g_dumpLeft.store(0); DVR_WARN("afw/dump: cannot write %s - capture stopped", path); return; }
    fprintf(meta, "fgFov=%.4f\nfgOn=%d\nfgDepth=%.4f\nfgMask=%d\nfreshMaskOk=%d\nheldMaskOk=%d\n", g_fgFov.load(), g_fgOn.load() ? 1 : 0,
            g_fgDepth.load(), g_fgMask.load() ? 1 : 0, fr.maskOk ? 1 : 0, src.maskOk ? 1 : 0);
    fprintf(meta, "present=%u\nheld=%d\nfresh=%d\nhaveHeld=%d\nuseFresh=%d\nuseHeld=%d\nmatrixVerdict=%d\nyawDeg=%.5f\n"
                  "tanH=%.7f\ntanV=%.7f\ntargetSize=%ux%u\nmPerUnit=%.6f\nuuPerUnit=%.3f\nworldScale=%.4f\nbodyUnits=%.4f\n"
                  "debug=%d\nstereo=%d\nmatrices=%d\n",
            n, held, 1 - held, haveH ? 1 : 0, useS ? 1 : 0, useT ? 1 : 0, verdict, yawDeg, tanH, tanV, w, h,
            dvr::clarity::depth_scale() / g_worldScale.load(), dvr::clarity::depth_scale(), g_worldScale.load(),
            g_bodyDepth.load(), g_debug.load() ? 1 : 0, g_stereo.load() ? 1 : 0, g_matrices.load() ? 1 : 0);
    meta_pose(meta, "target", tgt);
    meta_record(meta, "fresh", fr);
    if (haveH) meta_record(meta, "heldrec", src);
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s_fresh.raw", base);      dump_texture(dev, ctx, fr.tex, path, meta, "freshColor");
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s_fresh_depth.raw", base); dump_texture(dev, ctx, fr.dtex, path, meta, "freshDepth");
    if (haveH) {
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s_held.raw", base);      dump_texture(dev, ctx, src.tex, path, meta, "heldColor");
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s_held_depth.raw", base); dump_texture(dev, ctx, src.dtex, path, meta, "heldDepth");
    }
    // Run 15: the clean game images (before the mod's layers) the rebuild actually sampled, when it had them.
    if (fr.cleanOk && fr.ctex) { _snprintf_s(path, sizeof(path), _TRUNCATE, "%s_fresh_clean.raw", base); dump_texture(dev, ctx, fr.ctex, path, meta, "freshClean"); }
    if (haveH && src.cleanOk && src.ctex) { _snprintf_s(path, sizeof(path), _TRUNCATE, "%s_held_clean.raw", base); dump_texture(dev, ctx, src.ctex, path, meta, "heldClean"); }
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s_rebuilt.raw", base);    dump_texture(dev, ctx, dst, path, meta, "rebuilt");
    fclose(meta);
    const int left = g_dumpLeft.fetch_sub(1) - 1;
    if (left > 0) _snprintf_s(g_dumpStatus, sizeof(g_dumpStatus), _TRUNCATE, "capturing: %u of %u presents", n + 1, n + 1 + (uint32_t)left);
    else _snprintf_s(g_dumpStatus, sizeof(g_dumpStatus), _TRUNCATE, "done: %u presents in %s", n + 1, g_dumpDir);
    if (left <= 0) DVR_INFO("afw/dump: %u consecutive presents written to %s (native fresh eye, held eye, rebuilt held eye, "
                            "both depths and both records per present)", n + 1, g_dumpDir);
}
} // namespace

void request_dump(int presents, uint32_t delayMs, const char* dumpsRoot, const char* who) {
    if (presents <= 0 || presents > 32 || !dumpsRoot || !dumpsRoot[0]) {
        DVR_WARN("afw/dump: refused (%d presents, root %s) - 1..32 presents and a dumps folder", presents, dumpsRoot ? dumpsRoot : "none");
        return;
    }
    SYSTEMTIME t; GetLocalTime(&t);
    _snprintf_s(g_dumpDir, sizeof(g_dumpDir), _TRUNCATE, "%s\\afw-%04u%02u%02u-%02u%02u%02u", dumpsRoot, t.wYear, t.wMonth, t.wDay,
                t.wHour, t.wMinute, t.wSecond);
    CreateDirectoryA(g_dumpDir, nullptr);
    g_dumpIndex = 0;
    g_dumpAtMs.store(GetTickCount64() + delayMs);
    g_dumpLeft.store(presents);
    _snprintf_s(g_dumpStatus, sizeof(g_dumpStatus), _TRUNCATE, "armed: %d presents in %.1f s", presents, delayMs / 1000.0);
    DVR_INFO("afw/dump: armed (%s) - %d consecutive presents from %.1f s from now into %s; each stalls the frame on its "
             "readbacks (about 80 MB per present)", who ? who : "?", presents, delayMs / 1000.0, g_dumpDir);
}
const char* dump_status() { return g_dumpStatus; }

// The compose's depth output, sized for a target of w x h.
bool ensure_zout(ID3D11Device* dev, uint32_t w, uint32_t h) {
    if (g_zOutRtv && g_zOutW == w && g_zOutH == h) return true;
    rel(g_zOutRtv); rel(g_zOutSrv); rel(g_zOut); g_zOutW = g_zOutH = 0; g_zOutSeq = 0;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    td.Format = DXGI_FORMAT_R32_FLOAT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = dev->CreateTexture2D(&td, nullptr, &g_zOut);
    if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(g_zOut, nullptr, &g_zOutRtv);
    if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(g_zOut, nullptr, &g_zOutSrv);
    if (FAILED(hr)) {
        rel(g_zOutRtv); rel(g_zOutSrv); rel(g_zOut);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "afw/xrdepth: the rebuilt eye's depth target %ux%u failed (0x%08lx) - no depth layer",
                         w, h, (unsigned long)hr);
        return false;
    }
    g_zOutW = w; g_zOutH = h;
    DVR_INFO("afw/xrdepth: the rebuilt eye's depth target %ux%u (R32F, about %.1f MB)", w, h, w * h * 4.0 / 1048576.0);
    return true;
}

bool warp_held(ID3D11Device* dev, ID3D11DeviceContext* ctx, int held, int fresh, uint32_t freshSerial,
               ID3D11Texture2D* dst, uint32_t w, uint32_t h, float tanH, float tanV, Pose* outPose, const char** why) {
    beat();
    if (why) *why = nullptr;
    if (!g_on.load()) { if (why) *why = "off"; return false; }
    sync_epoch();
    if (!init(dev)) { ++g_notReady; if (why) *why = "not ready"; return false; }
    if (held < 0 || held > 1 || fresh != 1 - held || !dst || !ctx) { if (why) *why = "bad call"; return false; }
    poll_timestamps(ctx);
    Held& src = g_held[held];
    Held& fr = g_held[fresh];
    if (!fr.valid || !fr.srv) { ++g_noHeld; if (why) *why = "no fresh image"; return false; }
    // Fresh means THIS present's capture: the newest record AND the serial the capture delivered now.
    if (fr.seq != g_seq || fr.serial != freshSerial) {
        ++g_notFresh; if (why) *why = "the fresh eye was not captured this present"; return false;
    }
    const bool haveH = src.valid && src.srv && src.seq < fr.seq;
    for (Held* e : {&fr, &src})
        if ((e == &fr || haveH) && !e->depthOk) { if (snapshot_depth(dev, ctx, *e)) { ++g_snaps; ++g_snapLate; } else ++g_snapMiss; }
    const bool useS = g_stereo.load() && fr.depthOk && fr.dsrv;
    const bool useT = haveH && src.depthOk && src.dsrv;
    if (!useS && !useT) { ++g_noDepth; if (why) *why = "no depth for either image"; return false; }
    if (!ensure_seeds(dev, w, h)) { ++g_noSeed; if (why) *why = "no seed maps"; return false; }
    D3D11_TEXTURE2D_DESC dd;
    dst->GetDesc(&dd);
    D3D11_RENDER_TARGET_VIEW_DESC rv = {};
    rv.Format = typed(dd.Format);
    rv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    ID3D11RenderTargetView* rtv = nullptr;
    if (FAILED(dev->CreateRenderTargetView(dst, &rv, &rtv)) || !rtv) {
        ++g_noRtv; if (why) *why = "no render target"; return false;
    }
    // The target: the held eye's view pose of the FRESH image's locate generation.
    const Pose& tgt = fr.targets[held];
    float d = (haveH && src.bodyOk && fr.bodyOk) ? fr.bodyYaw - src.bodyYaw : 0.0f;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    struct CB {
        float s[3][4]; float sp[4]; float f[3][4]; float fp[4]; float d[3][4]; float dp[4];
        float y[3][4]; float yc[4]; float prm[4]; float prm2[4]; float prm3[4];
        float hI[3][4]; float hC[4]; float tA[4], tB[4], tW[4]; float mD[4]; float prm4[4]; float prm5[4]; float prm6[4];
        float hand[2][5][4]; float prm7[4]; float mY[3][4];
        float prm8[4]; float prm9[4];
    } cb;
    static_assert(sizeof(CB) == 46 * 16, "afw cbuffer layout");
    memset(&cb, 0, sizeof(cb));
    const Pose& hp = haveH ? src.pose : fr.pose;
    rows(hp, cb.s, false);
    cb.sp[0] = hp.p[0]; cb.sp[1] = hp.p[1]; cb.sp[2] = hp.p[2];
    rows(fr.pose, cb.f, false);
    cb.fp[0] = fr.pose.p[0]; cb.fp[1] = fr.pose.p[1]; cb.fp[2] = fr.pose.p[2];
    rows(tgt, cb.d, true);
    cb.dp[0] = tgt.p[0]; cb.dp[1] = tgt.p[1]; cb.dp[2] = tgt.p[2];
    // UE yaw turns right positive; about XR +Y a positive angle turns LEFT. The world content the body
    // turned away from lies to the left of the fresh view: Ry(+d) (the runtime's pose fallback agrees).
    const float a = d / 57.29578f, ca = cosf(a), sa = sinf(a);
    const float yr[3][4] = {{ca, 0, sa, 0}, {0, 1, 0, 0}, {-sa, 0, ca, 0}};
    memcpy(cb.y, yr, sizeof(yr));
    for (int k = 0; k < 3; ++k) cb.yc[k] = 0.5f * (fr.targets[0].p[k] + fr.targets[1].p[k]);
    const float body = g_bodyDepth.load();
    cb.prm[0] = tanH; cb.prm[1] = tanV;
    g_lastClaimDeg = 2.0f * atanf(tanH) * 57.29578f;
    cb.prm[2] = dvr::clarity::depth_scale() / g_worldScale.load();
    cb.prm[3] = body;
    // The consistency tolerance, in target texels, widened by the depth's own texel size: under an upscaler
    // the depth is at the render size (1.3-3x coarser), and a sample can only be as consistent as its depth.
    const float depthTexel = fr.dw ? fmaxf(1.0f, (float)w / (float)fr.dw) : 1.0f;
    cb.prm2[0] = (float)w; cb.prm2[1] = (float)h; cb.prm2[2] = 1.5f * depthTexel; cb.prm2[3] = 0;
    cb.prm3[0] = useS ? 1.0f : 0.0f; cb.prm3[1] = useT ? 1.0f : 0.0f; cb.prm3[2] = g_debug.load() ? 1.0f : 0.0f;
    cb.prm3[3] = g_staleTol.load();   // run 18: `afw stale <relative>`
    if (fr.dw && fr.dh) { cb.prm4[0] = (float)kGridStep / fr.dw; cb.prm4[1] = (float)kGridStep / fr.dh; }
    if (haveH && src.dw && src.dh) { cb.prm4[2] = (float)kGridStep / src.dw; cb.prm4[3] = (float)kGridStep / src.dh; }
    {   // The foreground's projection: the game camera FOV, same aspect as the layer's claim.
        const float fg = g_fgFov.load();
        if (g_fgOn.load() && fg > 10.0f && fg < 175.0f) {
            cb.prm5[0] = tanf(fg * 0.5f / 57.29578f);
            cb.prm5[1] = cb.prm5[0] * tanV / tanH;
            cb.prm5[2] = g_fgDepth.load();
            ++g_fgUsed;
        }
    }
    cb.prm5[3] = g_nearMiss.load();
    cb.prm6[0] = g_ownHands.load() > 0.0f ? 1.0f : 0.0f; cb.prm6[1] = g_ownHands.load();
    if (g_stillShade.load() && g_handsStill.load()) { cb.prm6[0] = 1.0f; cb.prm6[1] = 1e6f; ++g_stillUsed; }   // run 21: no colour limit
    // Run 15: the clean images (the fresh eye's hands without its UI; the held eye's UI found by difference).
    const bool freshClean = g_cleanOn.load() && fr.cleanOk && fr.csrv;
    const bool heldClean = g_cleanOn.load() && haveH && src.cleanOk && src.csrv;
    cb.prm8[0] = freshClean ? 1.0f : 0.0f; cb.prm8[1] = heldClean ? 1.0f : 0.0f; cb.prm8[2] = g_cleanUi.load();
    if (freshClean) ++g_cleanUsed;
    cb.prm9[0] = g_edgeHands.load() ? 1.0f : 0.0f; cb.prm9[1] = 0.25f;
    const bool maskOn = g_fgMask.load() && fr.maskOk && (!haveH || src.maskOk);
    cb.prm6[2] = maskOn ? 1.0f : 0.0f;
    // Run 25: the held eye's hands FOLLOW THEIR CONTROLLERS. Each image carries the grips it was drawn with (note_hands);
    // the held eye's foreground moves rigidly with the nearer grip from the held image's pose to the fresh image's
    // (MSW's handMove), so its own hands are where the hands are NOW and can fill what the fresh eye cannot see (a thumb
    // behind the palm, a finger's side, a blade's far side) while the hands move, not only while they are still. With the
    // drawn mask the foreground is known exactly, so the reach is the whole weapon (1.2 m), not MSW's 30 cm guard.
    bool heldHandsMoved = false;
    if (g_heldHandsFollow.load() && haveH) {
        for (int k = 0; k < 2; ++k)
            if (src.hands[k].ok && fr.hands[k].ok) { hand_motion(src.hands[k], fr.hands[k], cb.hand[k]); heldHandsMoved = true; }
        if (heldHandsMoved) { cb.prm7[0] = 1.0f; cb.prm7[3] = maskOn ? 1.2f * 1.2f : 0.30f * 0.30f; ++g_heldHandsUsed; }
        else ++g_heldHandsNoPose;
    }
    // Run 23 (still) / run 25 (moved by the controllers): hand parts the fresh eye cannot see keep the held eye's own.
    cb.prm9[2] = (g_edgeHands.load() && (g_handsStill.load() || heldHandsMoved)) ? 1.0f : 0.0f;
    if (maskOn) ++g_maskUsed;   // texels: the nearer candidate within this beats the fill
    int verdict = kUnused;
    if (useT) {
        verdict = matrix_world(src, fr, tgt, d, tanH, tanV, &cb.hI[0][0], cb.hC, cb.tA, cb.tB, cb.tW, cb.mD);
        if (verdict == kUsed) cb.prm2[3] = dvr::clarity::depth_scale();
    }
    g_matrixVerdict.store(verdict);
    ctx->UpdateSubresource(g_cb, 0, nullptr, &cb, 0, 0);

    Ts* ts = nullptr;
    if (g_tsOk) for (Ts& s : g_ts) if (!s.pending) { ts = &s; break; }
    Saved sv; sv.save(ctx);
    if (ts) { ctx->Begin(ts->dis); ctx->End(ts->a); }
    ID3D11ShaderResourceView* none[8] = {};
    ctx->PSSetShaderResources(0, 8, none);   // nothing of ours may stay bound while it becomes a target
    // The seed maps.
    for (int k = 0; k < 2; ++k) {
        const Held& e = k ? src : fr;
        const float clearSeed[4] = {0, 0, 0, 0};
        ctx->ClearRenderTargetView(g_seed[k].rtv, clearSeed);
        if (!(k ? useT : useS)) continue;
        ctx->ClearDepthStencilView(g_seedDsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        const uint32_t gw = (e.dw - 1 + kGridStep - 1) / kGridStep, gh = (e.dh - 1 + kGridStep - 1) / kGridStep;
        const float mcb[8] = {(float)k, (float)kGridStep, (float)e.dw, (float)e.dh, (float)gw, (float)g_seedW, (float)g_seedH, kStretchArea};
        ctx->UpdateSubresource(g_cbMesh, 0, nullptr, mcb, 0, 0);
        setup_draw(ctx, g_seed[k].rtv, g_seedDsv, g_seedW, g_seedH, g_vsMesh, g_psMesh);
        ID3D11Buffer* cbs[2] = {g_cb, g_cbMesh};
        ctx->VSSetConstantBuffers(0, 2, cbs);
        ctx->PSSetConstantBuffers(0, 2, cbs);
        ID3D11ShaderResourceView* vsrv[4] = {nullptr, useT ? src.dsrv : nullptr, nullptr, useS ? fr.dsrv : nullptr};
        ctx->VSSetShaderResources(0, 4, vsrv);
        ctx->Draw(6 * gw * gh, 0);
        ctx->VSSetShaderResources(0, 4, none);
    }
    // The compose (and, for the depth layer, the rebuilt eye's depth as a second target).
    setup_draw(ctx, rtv, nullptr, w, h, g_vs, g_ps);
    const bool zOut = g_xrDepthWanted.load() && ensure_zout(dev, w, h);
    if (zOut) {
        ID3D11RenderTargetView* two[2] = {rtv, g_zOutRtv};
        ctx->OMSetRenderTargets(2, two, nullptr);
        g_zOutSeq = fr.seq;
    }
    ID3D11Buffer* cbs[2] = {g_cb, g_cbMesh};
    ctx->PSSetConstantBuffers(0, 2, cbs);
    ID3D11ShaderResourceView* srvs[8] = {haveH ? src.srv : fr.srv, useT ? src.dsrv : nullptr, fr.srv, useS ? fr.dsrv : nullptr,
                                         g_seed[0].srv, g_seed[1].srv, freshClean ? fr.csrv : nullptr, heldClean ? src.csrv : nullptr};
    ctx->PSSetShaderResources(0, 8, srvs);
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 8, none);
    if (ts) { ctx->End(ts->b); ctx->End(ts->dis); ts->pending = true; }
    sv.restore(ctx);
    rtv->Release();

    (useS && useT ? g_warpsFull : useT ? g_warpsTemporal : g_warpsStereo)++;
    g_yawAbs += fabsf(d);
    if (fabsf(d) > g_yawMax) g_yawMax = fabsf(d);
    if (outPose) *outPose = tgt;
    dump_tick(dev, ctx, src, fr, haveH, held, dst, w, h, tanH, tanV, d, useS, useT, verdict, tgt);
    return true;
}

// ---- the mod's own spacewarp (MSW): a display slot the game did not fill --------------------------------
// An eye rebuilt from its OWN last image and depth for a later slot. What the runtime cannot do exactly is the
// translation (it needs depth), so the image keeps its own ORIENTATION and only its position moves: the head's
// translation to the slot's located eye position, plus the body's walking extrapolated from the last two
// images (the camera displacement their c5 pair shows, less what the XR eye poses explain, per millisecond).
// Walking and rendered body turn are extrapolated on the real submissions' display timeline, into world
// pixels only; the head's own rotation to the slot is the compositor's as always.
// The hands are held fixed in tracking space (the body hypothesis): a controller that moves inside one slot
// lags that slot. Disocclusions keep the image's own nearest sample (a few cm of translation per slot).
std::atomic<bool> g_synthHands{false};     // `vrpace msw hands on|off`: the hands follow their controllers in a synthesized slot
uint32_t g_synthHandsUsed = 0;
std::atomic<bool> g_synthExtrap{true};     // `vrpace msw extrap on|off`: the walking/turning extrapolation (off = head only)
uint32_t g_synths = 0, g_synthMtx = 0, g_synthRefused = 0;
float g_synthStepMax = 0, g_synthYawMax = 0;
uint32_t g_synthDisplayClock = 0, g_synthNoMotion = 0;
double g_synthAgeMax = 0, g_synthWriterDeltaMax = 0;
const char* g_synthWhy = "";

// The body's motion between the two newest images: walking (uu per ms, UE world) and turning (deg per ms).
const double kTurnSignMtx = -1.0, kTurnSignXr = 1.0;   // host-verified (the turn case)
// The synthesized slot's seed grid (source texels per cell). A slot moves the image by millimetres to a few
// centimetres, so the fixed-point search converges from a coarse seed; the grid pass is most of the cost.
std::atomic<int> g_synthGrid{4};
void set_synth_grid(int step) { if (step >= 2 && step <= 16) g_synthGrid.store(step); }
int synth_grid() { return g_synthGrid.load(); }
bool body_motion(double v[3], double* yawPerMs, double* dtMs, bool displayClock) {
    Held& a = g_held[0];
    Held& b = g_held[1];
    if (!a.valid || !b.valid || !a.vpOk || !b.vpOk) return false;
    const Held& N = a.seq > b.seq ? a : b;
    const Held& O = a.seq > b.seq ? b : a;
    if (displayClock && (N.meta.displayTime <= O.meta.displayTime || O.meta.displayTime <= 0)) return false;
    const double dt = displayClock ? (N.meta.displayTime - O.meta.displayTime) * 1e-6
                                   : N.meta.captureMs - O.meta.captureMs;
    if (!(dt > 0.5 && dt < 100.0)) return false;
    double fwd[3], right[3], up[3];
    if (!axes_of(lin_of(N.vp), fwd, right, up)) return false;
    const double scale = g_worldScale.load();
    double of[3], oright[3], ou[3];
    if (!axes_of(lin_of(O.vp), of, oright, ou)) return false;
    const double np[3] = {N.pose.p[0], N.pose.p[1], N.pose.p[2]}, op[3] = {O.pose.p[0], O.pose.p[1], O.pose.p[2]};
    double ne[3], oe[3]; rot(N.pose, np, ne, true); rot(O.pose, op, oe, true);
    for (int k = 0; k < 3; ++k) {
        // Each eye offset belongs to its own body orientation. Subtracting both through the
        // newest basis mistakes the orbit of an off-centre head during a turn for walking.
        const double newOffset = scale * (-ne[2] * fwd[k] + ne[0] * right[k] + ne[1] * up[k]);
        const double oldOffset = scale * (-oe[2] * of[k] + oe[0] * oright[k] + oe[1] * ou[k]);
        v[k] = ((-N.c5[k] - newOffset) - (-O.c5[k] - oldOffset)) / dt;
        if (!std::isfinite(v[k])) return false;
    }
    // Remove each image's OWN head rotation from its rendered basis. Camera-writer bodyYaw can
    // be a tick older than these pixels; using it predicts alternating stale/current turn speeds.
    auto rendered_body_yaw = [](const Held& image, double* yaw) {
        double f[3], r[3], u[3], local[3];
        if (!axes_of(lin_of(image.vp), f, r, u)) return false;
        const double trackingForward[3] = {0, 0, -1};
        rot(image.pose, trackingForward, local, true);
        double world[3];
        for (int k = 0; k < 3; ++k) world[k] = -local[2] * f[k] + local[0] * r[k] + local[1] * u[k];
        if (!(world[0] * world[0] + world[1] * world[1] > 0.5)) return false;
        *yaw = atan2(world[1], world[0]) * 57.29577951;
        return std::isfinite(*yaw);
    };
    double newYaw = 0, oldYaw = 0;
    const bool turnOk = N.bodyOk && O.bodyOk && rendered_body_yaw(N, &newYaw) && rendered_body_yaw(O, &oldYaw);
    double dy = turnOk ? newYaw - oldYaw : 0.0;
    while (dy > 180.0) dy -= 360.0;
    while (dy < -180.0) dy += 360.0;
    if (turnOk) {
        double difference = dy - (N.bodyYaw - O.bodyYaw);
        while (difference > 180.0) difference -= 360.0;
        while (difference < -180.0) difference += 360.0;
        g_synthWriterDeltaMax = fmax(g_synthWriterDeltaMax, fabs(difference));
    }
    *yawPerMs = dy / dt;
    *dtMs = dt;
    return true;
}

void note_hands(int eye, const HandPose hands[2]) {
    if (eye < 0 || eye > 1 || !hands) return;
    g_held[eye].hands[0] = hands[0]; g_held[eye].hands[1] = hands[1];
}
void set_synth_hands(bool on) {
    if (g_synthHands.exchange(on) != on)
        DVR_INFO("msw: hands %s", on ? "FOLLOW their controllers in a synthesized slot (each hand's pixels move rigidly with the "
                                     "nearer grip, from the image's pose to the slot's)" : "held where the image drew them (off)");
}
bool synth_hands() { return g_synthHands.load(); }

// The rigid motion of a grip from `a` to `b`, as the shader's rows: R = Rb Ra^T, origin a, destination b.
void hand_motion(const HandPose& a, const HandPose& b, float out[5][4]) {
    float ra[3][4], rb[3][4];
    Pose pa{{a.q[0], a.q[1], a.q[2], a.q[3]}, {a.p[0], a.p[1], a.p[2]}}, pb{{b.q[0], b.q[1], b.q[2], b.q[3]}, {b.p[0], b.p[1], b.p[2]}};
    rows(pa, ra, false); rows(pb, rb, false);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) out[r][c] = rb[r][0] * ra[c][0] + rb[r][1] * ra[c][1] + rb[r][2] * ra[c][2];
        out[r][3] = 0;
    }
    out[3][0] = a.p[0]; out[3][1] = a.p[1]; out[3][2] = a.p[2]; out[3][3] = 1;
    out[4][0] = b.p[0]; out[4][1] = b.p[1]; out[4][2] = b.p[2]; out[4][3] = 1;
}

void set_synth_extrapolate(bool on) {
    if (g_synthExtrap.exchange(on) != on)
        DVR_INFO("msw: body extrapolation %s (off: the synthesized slot follows the head only)", on ? "ON" : "off");
}
bool synth_extrapolate() { return g_synthExtrap.load(); }

bool synth_eye(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* dst, uint32_t w, uint32_t h,
               float tanH, float tanV, const float targetPos[3], double nowMs, Pose* outPose, const char** why,
               const HandPose* slotHands, int64_t displayTime) {
    if (why) *why = nullptr;
    auto refuse = [&](const char* r) { ++g_synthRefused; g_synthWhy = r; if (why) *why = r; return false; };
    if (!g_on.load()) return refuse("AFW off");
    if (eye < 0 || eye > 1 || !dst || !ctx || !dev) return refuse("bad call");
    sync_epoch();
    if (!init(dev)) return refuse("not ready");
    Held& own = g_held[eye];
    if (!own.valid || !own.srv || !own.depthOk || !own.dsrv) return refuse("no own image with depth");
    if (!ensure_seeds(dev, w, h)) return refuse("no seed maps");
    D3D11_TEXTURE2D_DESC dd;
    dst->GetDesc(&dd);
    D3D11_RENDER_TARGET_VIEW_DESC rv = {};
    rv.Format = typed(dd.Format);
    rv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    ID3D11RenderTargetView* rtv = nullptr;
    if (FAILED(dev->CreateRenderTargetView(dst, &rv, &rtv)) || !rtv) return refuse("no render target");

    // The target: the image's own orientation at the slot's eye position.
    Pose tgt = own.pose;
    tgt.p[0] = targetPos[0]; tgt.p[1] = targetPos[1]; tgt.p[2] = targetPos[2];
    const bool displayClock = displayTime > 0;
    const double dtOwn = displayClock ? (own.meta.displayTime > 0 ? (displayTime - own.meta.displayTime) * 1e-6 : -1.0)
                                      : nowMs - own.meta.captureMs;
    double v[3] = {0, 0, 0}, yawPerMs = 0, motionDt = 0;
    const bool extrap = g_synthExtrap.load() && dtOwn > 0.0 && dtOwn < 60.0 && body_motion(v, &yawPerMs, &motionDt, displayClock);
    if (displayClock) ++g_synthDisplayClock;
    if (g_synthExtrap.load() && !extrap) ++g_synthNoMotion;
    if (dtOwn > 0 && dtOwn < 1000) g_synthAgeMax = fmax(g_synthAgeMax, dtOwn);
    const double turn = extrap ? yawPerMs * dtOwn : 0.0;

    struct CB {
        float s[3][4]; float sp[4]; float f[3][4]; float fp[4]; float d[3][4]; float dp[4];
        float y[3][4]; float yc[4]; float prm[4]; float prm2[4]; float prm3[4];
        float hI[3][4]; float hC[4]; float tA[4], tB[4], tW[4]; float mD[4]; float prm4[4]; float prm5[4]; float prm6[4];
        float hand[2][5][4]; float prm7[4]; float mY[3][4];
        float prm8[4]; float prm9[4];   // run 15/22: zero here - the synthesized slot keeps its own composed image, no edge hands
    } cb;
    static_assert(sizeof(CB) == 46 * 16, "afw cbuffer layout");
    memset(&cb, 0, sizeof(cb));
    rows(own.pose, cb.s, false);
    cb.sp[0] = own.pose.p[0]; cb.sp[1] = own.pose.p[1]; cb.sp[2] = own.pose.p[2];
    rows(own.pose, cb.f, false);
    cb.fp[0] = own.pose.p[0]; cb.fp[1] = own.pose.p[1]; cb.fp[2] = own.pose.p[2];
    rows(tgt, cb.d, true);
    cb.dp[0] = tgt.p[0]; cb.dp[1] = tgt.p[1]; cb.dp[2] = tgt.p[2];
    // The extrapolated body turn goes into the IMAGE, the world only: the foreground (the hands, fixed in
    // tracking space) must not turn with it. Run 10: the turn was applied to the submitted pose, which turned the
    // hands too - smooth world, jittery hands while turning. Without the matrices, the XR model's yaw rows carry it.
    {
        const float a = (float)(kTurnSignXr * turn / 57.29578), ca = cosf(a), sa = sinf(a);
        const float yr[3][4] = {{ca, 0, sa, 0}, {0, 1, 0, 0}, {-sa, 0, ca, 0}};
        memcpy(cb.y, yr, sizeof(yr));
        cb.yc[0] = tgt.p[0]; cb.yc[1] = tgt.p[1]; cb.yc[2] = tgt.p[2];
    }
    cb.prm7[2] = 1.0f;   // the foreground ignores the world yaw
    cb.prm[0] = tanH; cb.prm[1] = tanV;
    cb.prm[2] = dvr::clarity::depth_scale() / g_worldScale.load();
    cb.prm[3] = g_bodyDepth.load();
    const float depthTexel = own.dw ? fmaxf(1.0f, (float)w / (float)own.dw) : 1.0f;
    cb.prm2[0] = (float)w; cb.prm2[1] = (float)h; cb.prm2[2] = 1.5f * depthTexel; cb.prm2[3] = 0;
    cb.prm3[0] = 0.0f; cb.prm3[1] = 1.0f; cb.prm3[2] = g_debug.load() ? 1.0f : 0.0f; cb.prm3[3] = 0.03f;
    if (own.dw && own.dh) { cb.prm4[2] = (float)g_synthGrid.load() / own.dw; cb.prm4[3] = (float)g_synthGrid.load() / own.dh; }
    {
        const float fg = g_fgFov.load();
        if (g_fgOn.load() && fg > 10.0f && fg < 175.0f) {
            cb.prm5[0] = tanf(fg * 0.5f / 57.29578f);
            cb.prm5[1] = cb.prm5[0] * tanV / tanH;
            cb.prm5[2] = g_fgDepth.load();
        }
    }
    cb.prm5[3] = g_nearMiss.load();
    cb.prm6[2] = (g_fgMask.load() && own.maskOk) ? 1.0f : 0.0f;
    if (g_synthHands.load() && slotHands) {   // the hands: each tracked grip's motion from the image to the slot
        bool any = false;
        for (int k = 0; k < 2; ++k)
            if (own.hands[k].ok && slotHands[k].ok) { hand_motion(own.hands[k], slotHands[k], cb.hand[k]); any = true; }
        if (any) { cb.prm7[0] = 1.0f; cb.prm7[3] = 0.30f * 0.30f; ++g_synthHandsUsed; }   // within 30 cm of a grip
    }
    // The world by the game's matrices: the same camera-relative projection (the orientation is the image's
    // own), the camera moved by the head's translation and the extrapolated walk.
    double step = 0;
    if (g_matrices.load() && !g_camFlipLatched && own.vpOk) {
        const Lin l = lin_of(own.vp);
        double I[3][3], fwd[3], right[3], up[3];
        if (inverse_rows(l, I) && axes_of(l, fwd, right, up)) {
            const double scale = g_worldScale.load();
            const double de[3] = {tgt.p[0] - own.pose.p[0], tgt.p[1] - own.pose.p[1], tgt.p[2] - own.pose.p[2]};
            double ev[3]; rot(own.pose, de, ev, true);
            const double targetTracking[3] = {tgt.p[0], tgt.p[1], tgt.p[2]};
            double targetLocal[3], targetWorld[3]; rot(own.pose, targetTracking, targetLocal, true);
            for (int k = 0; k < 3; ++k)
                targetWorld[k] = scale * (-targetLocal[2] * fwd[k] + targetLocal[0] * right[k] + targetLocal[1] * up[k]);
            const double ca = cos(turn / 57.29577951), sa = sin(turn / 57.29577951);
            const double orbit[3] = {(ca - 1) * targetWorld[0] - sa * targetWorld[1],
                                     sa * targetWorld[0] + (ca - 1) * targetWorld[1], 0};
            double D[3];
            for (int k = 0; k < 3; ++k) {
                const double walk = extrap ? v[k] * dtOwn : 0.0;
                D[k] = -(scale * (-ev[2] * fwd[k] + ev[0] * right[k] + ev[1] * up[k]) + walk + orbit[k]);
                step += walk * walk;
            }
            step = sqrt(step);
            for (int k = 0; k < 3; ++k) {
                for (int j = 0; j < 3; ++j) cb.hI[k][j] = (float)I[k][j];
                cb.hI[k][3] = 0;
            }
            cb.hC[0] = (float)l.A[3]; cb.hC[1] = (float)l.B[3]; cb.hC[2] = (float)l.W[3];
            for (int j = 0; j < 4; ++j) { cb.tA[j] = (float)l.A[j]; cb.tB[j] = (float)l.B[j]; cb.tW[j] = (float)l.W[j]; }
            for (int k = 0; k < 3; ++k) cb.mD[k] = (float)D[k];
            cb.prm2[3] = dvr::clarity::depth_scale();
            if (turn != 0.0) {   // the camera turned by `turn` about UE up: the relative point turns the other way
                const float a = (float)(kTurnSignMtx * turn / 57.29578), ca = cosf(a), sa = sinf(a);
                const float m[3][4] = {{ca, -sa, 0, 0}, {sa, ca, 0, 0}, {0, 0, 1, 0}};
                memcpy(cb.mY, m, sizeof(m));
                cb.prm7[1] = 1.0f;
            }
            ++g_synthMtx;
        }
    }
    ctx->UpdateSubresource(g_cb, 0, nullptr, &cb, 0, 0);

    Saved sv; sv.save(ctx);
    ID3D11ShaderResourceView* none[6] = {};
    ctx->PSSetShaderResources(0, 6, none);
    // One seed map: the image's own grid carried into the slot.
    const float clearSeed[4] = {0, 0, 0, 0};
    ctx->ClearRenderTargetView(g_seed[0].rtv, clearSeed);
    ctx->ClearRenderTargetView(g_seed[1].rtv, clearSeed);
    ctx->ClearDepthStencilView(g_seedDsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
    const uint32_t gs = (uint32_t)g_synthGrid.load();
    const uint32_t gw = (own.dw - 1 + gs - 1) / gs, gh = (own.dh - 1 + gs - 1) / gs;
    const float mcb[8] = {1.0f, (float)gs, (float)own.dw, (float)own.dh, (float)gw, (float)g_seedW, (float)g_seedH, kStretchArea};
    ctx->UpdateSubresource(g_cbMesh, 0, nullptr, mcb, 0, 0);
    setup_draw(ctx, g_seed[1].rtv, g_seedDsv, g_seedW, g_seedH, g_vsMesh, g_psMesh);
    ID3D11Buffer* cbs[2] = {g_cb, g_cbMesh};
    ctx->VSSetConstantBuffers(0, 2, cbs);
    ctx->PSSetConstantBuffers(0, 2, cbs);
    ID3D11ShaderResourceView* vsrv[4] = {nullptr, own.dsrv, nullptr, nullptr};
    ctx->VSSetShaderResources(0, 4, vsrv);
    ctx->Draw(6 * gw * gh, 0);
    ctx->VSSetShaderResources(0, 4, none);
    // The compose (and its depth, for the depth layer).
    setup_draw(ctx, rtv, nullptr, w, h, g_vs, g_ps);
    if (g_xrDepthWanted.load() && ensure_zout(dev, w, h)) {
        ID3D11RenderTargetView* two[2] = {rtv, g_zOutRtv};
        ctx->OMSetRenderTargets(2, two, nullptr);
        g_zOutSeq = g_seq;
    }
    ctx->PSSetConstantBuffers(0, 2, cbs);
    ID3D11ShaderResourceView* srvs[6] = {own.srv, own.dsrv, own.srv, nullptr, g_seed[0].srv, g_seed[1].srv};
    ctx->PSSetShaderResources(0, 6, srvs);
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 6, none);
    sv.restore(ctx);
    rtv->Release();

    // The submitted pose: the image's own orientation at the slot's eye position (the turn is in the image).
    if (outPose) *outPose = tgt;
    ++g_synths;
    if (step > g_synthStepMax) g_synthStepMax = (float)step;
    if (fabs(turn) > g_synthYawMax) g_synthYawMax = (float)fabs(turn);
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "msw: beat - %u eyes synthesized (%u with the game's matrices, %u with the hands moved by their controllers), %u "
        "refused (last: %s); extrapolation %s: walk up to %.1f uu and turn up to %.2f deg per synthesized eye (0 while "
        "standing still); display-clock eyes %u, prediction unavailable %u; source-to-slot max %.2f ms, "
        "rendered/written turn-step difference max %.3f deg (cumulative)",
        g_synths, g_synthMtx, g_synthHandsUsed, g_synthRefused, g_synthWhy[0] ? g_synthWhy : "none",
        g_synthExtrap.load() ? "on" : "off", g_synthStepMax, g_synthYawMax, g_synthDisplayClock, g_synthNoMotion,
        g_synthAgeMax, g_synthWriterDeltaMax);
    return true;
}

bool copy_own(ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* dst, Pose* pose) {
    if (!ctx || !dst || eye < 0 || eye > 1) return false;
    const Held& h = g_held[eye];
    if (!h.valid || !h.tex) return false;
    D3D11_TEXTURE2D_DESC a, b;
    h.tex->GetDesc(&a); dst->GetDesc(&b);
    if (a.Width != b.Width || a.Height != b.Height) return false;
    ctx->CopyResource(dst, h.tex);
    if (pose) *pose = h.pose;
    return true;
}

bool has_held(int held) {
    // The fresh image is what a rebuild needs; the held one is optional (without it: the fresh eye alone).
    return g_on.load() && held >= 0 && held <= 1 && g_held[1 - held].valid && g_held[1 - held].tex;
}

bool copy_held(ID3D11DeviceContext* ctx, int held, ID3D11Texture2D* dst) {
    if (!ctx || !dst || held < 0 || held > 1) return false;
    const Held& src = g_held[held].valid && g_held[held].tex ? g_held[held] : g_held[1 - held];
    if (!src.valid || !src.tex) return false;
    D3D11_TEXTURE2D_DESC a, b;
    src.tex->GetDesc(&a); dst->GetDesc(&b);
    if (a.Width != b.Width || a.Height != b.Height) return false;
    ctx->CopyResource(dst, src.tex);
    return true;
}

void set_xr_depth_wanted(bool on) {
    if (g_xrDepthWanted.exchange(on) != on)
        DVR_INFO("afw/xrdepth: the rebuilt eye's depth %s (the depth layer for the runtime's reprojection)", on ? "WRITTEN" : "not written");
}

bool write_xr_depth(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, bool rebuilt, ID3D11Texture2D* dst,
                    uint32_t dxgiFormat, uint32_t w, uint32_t h, float nearM, float farM, const char** why) {
    if (why) *why = nullptr;
    auto refuse = [&](const char* r) { ++g_xrRefused; g_xrLastWhy = r; if (why) *why = r; return false; };
    if (!g_on.load()) return refuse("AFW off");
    if (!dev || !ctx || !dst || eye < 0 || eye > 1) return refuse("bad call");
    if (!init(dev)) return refuse("not ready");
    ID3D11ShaderResourceView* src = nullptr;
    if (rebuilt) {
        // The rebuilt eye: the compose's own output, written for THIS present's fresh record.
        if (!g_zOutSrv || g_zOutSeq != g_seq || g_zOutW != w || g_zOutH != h) return refuse("no rebuilt depth this present");
        src = g_zOutSrv;
    } else {
        // An eye shown as captured: its own depth snapshot (copy_held's choice of record for a held eye).
        Held& e = g_held[eye].valid && g_held[eye].tex ? g_held[eye] : g_held[1 - eye];
        if (!e.valid) return refuse("no image record");
        if (!e.depthOk && !snapshot_depth(dev, ctx, e)) return refuse("no depth snapshot");
        if (!e.dsrv) return refuse("no depth snapshot");
        src = e.dsrv;
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC dv = {};
    dv.Format = (DXGI_FORMAT)dxgiFormat;
    dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ID3D11DepthStencilView* dsv = nullptr;
    if (FAILED(dev->CreateDepthStencilView(dst, &dv, &dsv)) || !dsv) return refuse("no depth view on the swapchain image");
    const float xp[4] = {dvr::clarity::depth_scale() / g_worldScale.load(), nearM, farM, 0};
    ctx->UpdateSubresource(g_cbXr, 0, nullptr, xp, 0, 0);
    Saved sv; sv.save(ctx);
    ID3D11ShaderResourceView* none[8] = {};
    ctx->PSSetShaderResources(0, 8, none);
    setup_draw(ctx, nullptr, dsv, w, h, g_vs, g_psXrDepth);
    ctx->OMSetRenderTargets(0, nullptr, dsv);
    ctx->OMSetDepthStencilState(g_dsAlways, 0);
    ID3D11Buffer* cbs[3] = {g_cb, g_cbMesh, g_cbXr};
    ctx->PSSetConstantBuffers(0, 3, cbs);
    ctx->PSSetShaderResources(0, 1, &src);
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 8, none);
    ID3D11Buffer* nocb[3] = {};
    ctx->PSSetConstantBuffers(0, 3, nocb);
    sv.restore(ctx);
    dsv->Release();
    ++g_xrWrites[eye];
    if (rebuilt) ++g_xrRebuilt;
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000, "afw/xrdepth: beat - depth layer written L %u R %u (rebuilt eye %u), refused %u (last: %s); "
                     "%.3f m per unit, near %.2f m, far %.0f m (the sky and anything farther sit at far)",
                     g_xrWrites[0], g_xrWrites[1], g_xrRebuilt, g_xrRefused, g_xrLastWhy[0] ? g_xrLastWhy : "none",
                     xp[0], nearM, farM);
    return true;
}

void shutdown() {
    for (Held& h : g_held) { rel(h.srv); rel(h.tex); rel(h.drtv); rel(h.dsrv); rel(h.dtex); rel(h.csrv); rel(h.ctex); h = Held{}; }
    rel(g_clean.srv); rel(g_clean.tex); g_clean = CleanCopy{};
    for (Ts& s : g_ts) { rel(s.dis); rel(s.a); rel(s.b); s.pending = false; }
    for (SeedMap& m : g_seed) { rel(m.rtv); rel(m.srv); rel(m.tex); }
    rel(g_seedDsv); rel(g_seedDepth); g_seedW = g_seedH = 0;
    rel(g_vs); rel(g_vsMesh); rel(g_ps); rel(g_psMesh); rel(g_psDepth); rel(g_psDepthMask); rel(g_psDepthK); rel(g_cb); rel(g_cbMesh);
    rel(g_lin); rel(g_point); rel(g_rs); rel(g_bs); rel(g_ds); rel(g_dsTest);
    rel(g_dsAlways); rel(g_psXrDepth); rel(g_cbXr); rel(g_zOutRtv); rel(g_zOutSrv); rel(g_zOut);
    g_zOutW = g_zOutH = 0; g_zOutSeq = 0;
    g_ready = false; g_failed = false; g_tsOk = false; g_seq = 0;
}

} // namespace dvr::afw
