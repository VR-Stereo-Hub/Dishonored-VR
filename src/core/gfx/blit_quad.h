// core/gfx/blit_quad.h - a full-target textured blit on the mod's D3D11 device
// (41.0). The captured game frame is BGRA; the runtime's eye swapchains are
// R8G8B8A8, and CopyResource needs the same typeless family, so every method
// resolves its output through one draw of a full-screen triangle (no vertex
// buffer, no input layout). The shaders compile at first use through
// d3dcompiler_47 (LoadLibrary'd, like d3d11 itself: the proxy adds no import).
#pragma once
#include <stdint.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11SamplerState;
struct ID3D11RasterizerState;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;

namespace dvr::gfx {

// VR-119: how the HUD blit derives the alpha the compositor blends with. The
// source is a sink cleared to transparent black with the HUD drawn over it, so
// its colour is premultiplied by construction; what its ALPHA channel holds
// depends on whether the redirect forced a coverage equation on the draws.
//   mode 0 repair    alpha = max(r,g,b)      (41.2; black strokes go faint)
//   mode 1 captured  alpha = the sink's A     (needs the forced separate-alpha blend)
//   mode 2 mix       alpha = max(A, repair*mixK)
// Then gain (multiply), floor (a minimum for any pixel with colour, keeps thin
// strokes), gamma (a nudge on the colour), and a backdrop plate composed UNDER
// the HUD in premultiplied form. Every value at identity reproduces 41.2.
struct AlphaParams {
    float ellipse[4] = {0,0,0,0}; // center UV and radii; zero radii disable
    int   mode = 0;
    float gain = 1.0f, floorA = 0.0f, gamma = 1.0f, mixK = 1.0f;
    float backdrop[4] = {0, 0, 0, 0};   // r, g, b, a (a = 0: no plate)
};

class BlitQuad {
public:
    bool init(ID3D11Device* dev);      // idempotent; false = shaders unavailable
    bool ready() const { return ready_; }
    void shutdown();
    // Draws `src` over the whole of `dst` (w x h). Sets every state it uses.
    // With `alpha` the HUD pixel shader runs (see AlphaParams); without it the
    // opaque copy. Falls back to the opaque shader when the variant did not
    // compile.
    void draw(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
              ID3D11RenderTargetView* dst, uint32_t w, uint32_t h, const AlphaParams* alpha = nullptr);
    bool alpha_ready() const { return psAlpha_ != nullptr && cb_ != nullptr; }
private:
    bool ready_ = false;
    bool failed_ = false;
    ID3D11VertexShader*      vs_ = nullptr;
    ID3D11PixelShader*       ps_ = nullptr;
    ID3D11PixelShader*       psAlpha_ = nullptr;
    ID3D11Buffer*            cb_ = nullptr;
    ID3D11SamplerState*      sampler_ = nullptr;
    ID3D11RasterizerState*   raster_ = nullptr;
    ID3D11BlendState*        blend_ = nullptr;
    ID3D11DepthStencilState* depth_ = nullptr;
};

} // namespace dvr::gfx
