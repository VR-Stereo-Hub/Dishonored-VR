// core/gfx/clarity_gpu.h - the clarity passes on a D3D11 device, with nothing
// of the mod in them, so tools/clarity-gpu-tests.cpp runs the production
// shaders on the host GPU (clarity.h is the mod's side: settings, the eye's
// pose record, the log).
//
// The chain, each step optional, every intermediate in linear light:
//   resolve   the render (w x h) filtered down to (ow x oh) with a Mitchell
//             kernel scaled to the step, two separable passes
//   temporal  blended with the same eye's previous output, reprojected by the
//             rotation between the two rendered cameras, the history clipped
//             to the current 3x3 neighbourhood's colour spread (YCoCg)
//   final     contrast-adaptive sharpening (0 = none), then back to the
//             gamma-encoded values the swapchain expects
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

namespace dvr::clarity {

struct PassParams {
    bool     srcGamma = true;    // the source holds gamma-encoded values (the game's capture)
    uint32_t w = 0, h = 0;       // source size
    uint32_t ow = 0, oh = 0;     // output size (equal to w x h without a resolve)
    bool     resolve = false;
    bool     temporal = false;
    int      eye = 0;            // history slot: 0 left, 1 right
    bool     historyValid = false;   // false seeds the history with this frame
    Mat3     prevFromCur = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    float    tanH = 1.0f, tanV = 1.0f;
    float    blend = 0.15f;      // weight of the current frame
    float    clipGamma = 1.0f;   // variance clip width in standard deviations
    float    sharpen = 0.0f;     // 0 = none, up to 1
    float    kernelB = 0.0f, kernelC = 0.5f;   // resolve kernel: Catmull-Rom (1/3, 1/3 = Mitchell)
};

class Gpu {
public:
    // Compiles the shaders and makes the state objects. False with a reason.
    bool init(ID3D11Device* dev, char* why, size_t cap);
    bool ready() const { return ready_; }
    void shutdown();
    // Runs the enabled passes and writes the final gamma image into `dst`
    // (ow x oh). False with a reason: nothing was drawn into dst.
    bool run(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
             ID3D11RenderTargetView* dst, const PassParams& p, char* why, size_t cap);
    // The history the next temporal pass for `eye` reads (null before one ran).
    ID3D11ShaderResourceView* history(int eye) const;
    uint64_t bytes() const { return bytes_; }   // intermediate memory held

private:
    struct Target {
        ID3D11Texture2D* tex = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        uint32_t w = 0, h = 0;
    };
    bool ensure(ID3D11Device* dev, Target& t, uint32_t w, uint32_t h, char* why, size_t cap);
    void release(Target& t);
    void pass(ID3D11DeviceContext* ctx, ID3D11PixelShader* ps, ID3D11ShaderResourceView* t0,
              ID3D11ShaderResourceView* t1, ID3D11RenderTargetView* dst, uint32_t w, uint32_t h);

    bool ready_ = false, failed_ = false;
    ID3D11VertexShader* vs_ = nullptr;
    ID3D11PixelShader* psResolveH_ = nullptr;
    ID3D11PixelShader* psResolveV_ = nullptr;
    ID3D11PixelShader* psTemporal_ = nullptr;
    ID3D11PixelShader* psFinal_ = nullptr;
    ID3D11Buffer* cb_ = nullptr;
    ID3D11SamplerState* linear_ = nullptr;
    ID3D11RasterizerState* raster_ = nullptr;
    ID3D11BlendState* blend_ = nullptr;
    ID3D11DepthStencilState* depth_ = nullptr;
    Target tmpH_, lin_;
    Target hist_[2][2];
    int histRead_[2] = {0, 0};
    bool histHave_[2] = {false, false};
    uint64_t bytes_ = 0;
};

} // namespace dvr::clarity
