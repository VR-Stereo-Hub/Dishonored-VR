// core/gfx/motion_gpu.h - motion vectors from depth and two cameras, and the instrument that
// proves them (docs/dishonored/PLAN-motion-vectors-dlss.md, step 3). No mod dependencies: the
// host test (tools/motion-gpu-tests.cpp) runs these shaders against a synthetic scene.
//
// A pixel of the current eye image at depth d (the scene target's alpha, in depth units) is the
// point  d * s * (F + x tanH R + y tanV U)  from the current camera, s = uu per depth unit. In the
// previous camera of the same eye that point is  q = M (d s dir) + t,  M = prev_from_cur and
// t = the current camera position minus the previous one, in the previous camera's axes (uu).
// Its previous image position is q's projection.
//
// THE INSTRUMENT: at a sparse grid, for each of kScales candidate s (the last one huge, which is
// rotation-only reprojection), the current pixel's luminance against the previous image's at the
// predicted position. With the camera moved, the right s has the least error, and depth that is
// not depth (or the wrong camera pair) leaves the curve flat or puts rotation-only on top.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "core/gfx/clarity_math.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11SamplerState;
struct ID3D11RasterizerState;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;

namespace dvr::motion {

const int kScales = 12;
const float kScaleValues[kScales] = {25, 50, 100, 200, 400, 700, 1000, 1500, 2500, 4000, 7000, 1.0e6f};
const int kGridW = 64, kGridH = 64;

struct CalibParams {
    dvr::clarity::Mat3 prevFromCur;
    float t[3] = {0, 0, 0};         // current minus previous camera position, previous camera axes (uu)
    float tanH = 1, tanV = 1;
    uint32_t w = 0, h = 0;          // the colour and depth size
    float farDepth = 1000.0f;       // depth at or above this is sky: skipped
    bool curGamma = true;           // the colours hold gamma-encoded values (luminance only; kept for clarity)
};

class CalibGpu {
public:
    bool init(ID3D11Device* dev, char* why, size_t cap);
    void shutdown();
    // cur, prev: colour of this and the previous frame of one eye (w x h). depth: the scene
    // target (alpha = depth). Writes the mean |luminance error| per scale and the samples used.
    bool run(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* cur,
             ID3D11ShaderResourceView* prev, ID3D11ShaderResourceView* depth, const CalibParams& p,
             float err[kScales], int* samples, char* why, size_t cap);
private:
    bool ready_ = false, failed_ = false;
    ID3D11VertexShader* vs_ = nullptr;
    ID3D11PixelShader* ps_ = nullptr;
    ID3D11Buffer* cb_ = nullptr;
    ID3D11SamplerState* linear_ = nullptr;
    ID3D11RasterizerState* raster_ = nullptr;
    ID3D11BlendState* blend_ = nullptr;
    ID3D11DepthStencilState* depth_ = nullptr;
    ID3D11Texture2D* out_ = nullptr;      // (kGridW * kScales) x kGridH RG32F: error, weight
    ID3D11RenderTargetView* outRtv_ = nullptr;
    ID3D11Texture2D* stage_ = nullptr;
};

// The CPU form of the shader's reprojection, for the host test.
inline bool reproject_depth(const dvr::clarity::Mat3& m, const float t[3], float tanH, float tanV,
                            float u, float v, float depthUu, float* pu, float* pv) {
    const float x = u * 2.0f - 1.0f, y = 1.0f - v * 2.0f;
    const float d[3] = {depthUu, x * tanH * depthUu, y * tanV * depthUu};
    float q[3];
    for (int i = 0; i < 3; ++i) q[i] = m.m[i][0] * d[0] + m.m[i][1] * d[1] + m.m[i][2] * d[2] + t[i];
    if (q[0] <= 1.0f) return false;
    *pu = 0.5f + 0.5f * (q[1] / (q[0] * tanH));
    *pv = 0.5f - 0.5f * (q[2] / (q[0] * tanV));
    return true;
}

} // namespace dvr::motion
