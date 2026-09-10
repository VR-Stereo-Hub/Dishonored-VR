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
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11SamplerState;
struct ID3D11RasterizerState;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;

namespace dvr::gfx {

class BlitQuad {
public:
    bool init(ID3D11Device* dev);      // idempotent; false = shaders unavailable
    bool ready() const { return ready_; }
    void shutdown();
    // Draws `src` over the whole of `dst` (w x h). Sets every state it uses.
    // alphaRepair writes alpha = max(r,g,b) instead of 1 and keeps the colour as
    // it is: for a source cleared to black and drawn over, that is premultiplied
    // coverage, which is what the runtime's HUD quad wants. Falls back to the
    // opaque shader when the variant did not compile.
    void draw(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
              ID3D11RenderTargetView* dst, uint32_t w, uint32_t h, bool alphaRepair = false);
private:
    bool ready_ = false;
    bool failed_ = false;
    ID3D11VertexShader*      vs_ = nullptr;
    ID3D11PixelShader*       ps_ = nullptr;
    ID3D11PixelShader*       psAlpha_ = nullptr;
    ID3D11SamplerState*      sampler_ = nullptr;
    ID3D11RasterizerState*   raster_ = nullptr;
    ID3D11BlendState*        blend_ = nullptr;
    ID3D11DepthStencilState* depth_ = nullptr;
};

} // namespace dvr::gfx
