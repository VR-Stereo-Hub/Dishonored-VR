// core/gfx/dlss_gpu.h - the DLSS guide images from what the mod already has: the scene
// depth shared from D3D9 (the eye-size RGBA16F target's alpha, linear view depth in the
// game's depth units, depth_probe.h) and the two cameras of an eye from its pose records
// (clarity_math.h). No mod dependencies, like clarity_gpu.
//
//   depth   R32_FLOAT, reversed: 1 / (1 + z) for scene depth z, 0 for sky (z >= 1000) and
//           for texels with no depth. DLSS gets no projection matrix, so it needs only an
//           ordering that is monotonic and consistent frame to frame, which this is. It is
//           NOT the engine's device depth; nothing else may read it as one.
//   motion  R16G16_FLOAT, previous UV minus current UV, the same reprojection the fused TAA
//           uses (clarity_gpu's Reproject): rotation for every pixel, plus translation
//           parallax where depth is known and not sky. Not clipped to the screen: a pixel
//           whose previous position was off screen gets its true vector, and DLSS decides.
//           Camera motion only - moving objects, hands and particles carry the camera's
//           vector, not their own (dlss.h, limits).
// tools/dlss-host-tests.cpp proved the sign: DLSS reads previous-minus-current.
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
struct ID3D11RasterizerState;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;

namespace dvr::dlss {

struct GuideParams {
    uint32_t w = 0, h = 0;                // the eye image (and guide) size
    bool historyValid = false;            // false: vectors are zero (DLSS is also reset)
    dvr::clarity::Mat3 prevFromCur = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    float translation[3] = {};            // current minus previous position, previous camera axes (uu)
    float tanH = 1, tanV = 1, prevTanH = 0, prevTanV = 0;
    ID3D11ShaderResourceView* sceneDepth = nullptr;   // alpha = linear depth; null = rotation only, depth 0
    uint32_t depthW = 0, depthH = 0;
    float depthScale = 200.0f;            // uu per depth unit (the coarse measured minimum)
};

class GuideGpu {
public:
    bool init(ID3D11Device* dev, char* why, size_t cap);
    void shutdown();
    bool run(ID3D11Device* dev, ID3D11DeviceContext* ctx, const GuideParams& p, char* why, size_t cap);
    ID3D11Texture2D* depth() const { return depth_; }
    ID3D11Texture2D* motion() const { return motion_; }
    uint64_t bytes() const { return (uint64_t)w_ * h_ * 8; }
private:
    bool ensure(ID3D11Device* dev, uint32_t w, uint32_t h, char* why, size_t cap);
    bool ready_ = false;
    ID3D11VertexShader* vs_ = nullptr;
    ID3D11PixelShader* ps_ = nullptr;
    ID3D11Buffer* cb_ = nullptr;
    ID3D11RasterizerState* raster_ = nullptr;
    ID3D11BlendState* blend_ = nullptr;
    ID3D11DepthStencilState* ds_ = nullptr;
    ID3D11Texture2D* depth_ = nullptr;
    ID3D11Texture2D* motion_ = nullptr;
    ID3D11RenderTargetView* depthRtv_ = nullptr;
    ID3D11RenderTargetView* motionRtv_ = nullptr;
    uint32_t w_ = 0, h_ = 0;
};

} // namespace dvr::dlss
