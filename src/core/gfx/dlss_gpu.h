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
//           Camera motion only - moving objects and particles carry the camera's vector.
//           EXCEPT the body: pixels nearer than bodyDepth are the first-person arms and
//           weapon, which walk with the player. They keep the rotation (the hands stay put in
//           the room while the head turns) and drop the translation parallax. Measured in the
//           simulator while walking: that band's vector error was 3.2x its no-motion error with
//           the parallax, every farther band improved (PERFORMANCE.md, the smear audit).
//   bias    R8_UNORM, DLSS's "bias current colour" mask: 1 where this eye's previous image,
//           moved by the vectors above, lands OUTSIDE the colour range of the current 3x3
//           neighbourhood - the vectors do not explain what is there (the game's arms and
//           weapon while walking, NPCs, parallax the coarse depth scale gets wrong). DLSS then
//           takes those pixels from the current image instead of smearing its history over
//           them. Anything the vectors do explain, including a sub-pixel shimmering edge,
//           stays inside the box and keeps its full accumulation.
// tools/dlss-host-tests.cpp proved the vector sign (DLSS reads previous-minus-current) and
// that a raised mask removes a trail DLSS otherwise keeps.
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
struct ID3D11ComputeShader;
struct ID3D11UnorderedAccessView;
struct ID3D11RasterizerState;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11SamplerState;

namespace dvr::dlss {

struct GuideParams {
    ID3D11ShaderResourceView* preFg = nullptr;   // VR-39: the scene target when the foreground pass began (the hands mask)
    uint32_t w = 0, h = 0;                // the eye image (and guide) size
    bool historyValid = false;            // false: vectors are zero (DLSS is also reset)
    dvr::clarity::Mat3 prevFromCur = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    float translation[3] = {};            // current minus previous position, previous camera axes (uu)
    float tanH = 1, tanV = 1, prevTanH = 0, prevTanV = 0;
    ID3D11ShaderResourceView* sceneDepth = nullptr;   // alpha = linear depth; null = rotation only, depth 0
    uint32_t depthW = 0, depthH = 0;
    float depthScale = 250.0f;            // uu per depth unit (the flow-check fit)
    float bodyDepth = 0.3f;               // depth units: nearer is the player's own arms/weapon (0 = off)
    // The game's own world view-projections (c0..c3 at the c5 upload, camera-relative, row
    // vector: clip = [P - C, 1] * M), current and previous image of this eye. When set, the
    // vectors reproject through them instead of the rotator/FOV reconstruction above.
    bool useVp = false;
    float vpCur[16] = {}, vpPrev[16] = {};
    float camDelta[3] = {};               // current minus previous camera position, world (uu)
    // The projection jitter each image was drawn with (pose record, dlss_jitter.h): sample offset
    // in render pixels. The vectors above never contain it; DLSS gets it separately, and the flow
    // check expects the image to have moved by (jitter - prevJitter) on top of the vectors.
    float jitter[2] = {}, prevJitter[2] = {};
    bool jitterKnown = false;             // both images' records were read
};

// The in-game audit of the vectors (sparse grid, read back without stalling): mean luminance
// error between the current image and the previous one moved by the vectors, against the same
// with no movement, binned by scene depth. A bin whose vector error is not well below its
// zero-motion error is where the vectors are wrong. `masked` is the mean bias mask there.
struct AuditBin { double vecErr = 0, zeroErr = 0, masked = 0; uint64_t n = 0; };

// The flow check: at each audit point, a 9x9-pixel block search (5x5 luminance patches, sub-pixel
// by parabola) around where the vector says the pixel came from finds where it really came
// from. err = found minus predicted, in render pixels. gain = true motion over predicted motion
// per axis (least squares): 1 = right, 0.5 or 2 = the vectors are timed a frame off, and a
// steady bias is an offset. Only confident, textured points count; arms (nearer than the body
// depth) are kept apart.
struct FlowStats {
    uint64_t n = 0, rejected = 0;
    double ex = 0, ey = 0, eabs = 0;          // sums of err x, err y, |err|
    double mt[2] = {}, mm[2] = {};            // sum of predicted*true and predicted^2 per axis
    double pabs = 0, tabs = 0;                // sums of |predicted| and |true|
    uint64_t big = 0;                         // points with |err| > 1 px
    uint64_t armsN = 0; double armsAbs = 0;   // the body band, kept apart
    // Per audited image: |its mean error| (a whole-image shift = the pose the image was rendered
    // with differs from the recorded one) and the mean distance of its points from that mean
    // (per-pixel scatter = depth, projection or matching noise).
    // With projection jitter the image also moves by the recorded jitter change: every error
    // above is AFTER subtracting it (so it stays the vectors' error). What the uncorrected image
    // did is kept here: the whole-image shift before the subtraction, and the jitter gain = the
    // mean shift along the expected jitter change over its length (1 = the image moved by exactly
    // the recorded offset, 0 = it did not move, -1 = the opposite way).
    uint64_t frames = 0; double frameShift = 0, frameScatter = 0;
    uint64_t jitFrames = 0; double jitShiftRaw = 0, jitExpect = 0, jitDot = 0, jitNorm = 0;
    // Per depth band (the audit's bins): true/predicted gain along the predicted direction and
    // mean error. A gain that drifts with depth means the depth is not linear in scene alpha.
    double bandMt[8] = {}, bandMm[8] = {}, bandErr[8] = {}; uint64_t bandN[8] = {};
};
const int kAuditBins = 8;   // depth units
const char* const kAuditBinNames[kAuditBins] = {"<0.1", "0.1-0.3", "0.3-1", "1-2", "2-10", "10-50", "50-1000", "sky"};

class GuideGpu {
public:
    bool init(ID3D11Device* dev, char* why, size_t cap);
    void shutdown();
    bool run(ID3D11Device* dev, ID3D11DeviceContext* ctx, const GuideParams& p, char* why, size_t cap);
    ID3D11Texture2D* depth() const { return depth_; }
    ID3D11Texture2D* motion() const { return motion_; }
    // The bias mask for this eye image, from run()'s vectors and this eye's previous colour.
    // historyValid false (or no previous colour yet) writes zero. lo/hi: the colour excess
    // outside the 3x3 range (0..1, gamma) where the mask starts and saturates.
    // colourPart: the colour-change mask; sceneDepth + preFg (both or neither): the foreground always masked.
    bool mask(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
              bool historyValid, float lo, float hi, char* why, size_t cap, bool colourPart = true,
              ID3D11ShaderResourceView* sceneDepth = nullptr, ID3D11ShaderResourceView* preFg = nullptr);
    ID3D11Texture2D* bias() const { return bias_; }
    // Keeps this eye image as the eye's previous colour (after mask() and the audit read it).
    bool keep(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* color);
    void forget(int eye);   // the eye's history no longer belongs to the next image
    // Queues a sparse audit of this eye image (needs run() and mask() first) and folds in the
    // oldest finished one, never waiting for the GPU.
    // expX/Y: where the image moved by jitter alone (current minus previous sample offset, render px).
    void audit(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
               float expX = 0, float expY = 0);
    // VR-39: object motion - the camera vectors corrected where this eye's image and its previous one show
    // something moving on its own or with the camera (characters, a boat). Needs run() and this eye's previous
    // colour (keep()); overwrites motion(). jitShift: current minus previous sample offset (render px).
    struct ObjParams { float ratio = 0.5f, minGain = 0.02f, minContrast = 0.04f; bool temporal = true; };
    bool objmotion(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
                   float jitShiftX, float jitShiftY, const ObjParams& op, char* why, size_t cap);
    AuditBin bins[kAuditBins];
    FlowStats flow;
    float bodyDepth = 0.3f;   // the arms/weapon band the flow check keeps apart
    uint64_t bytes() const;
private:
    bool ensure(ID3D11Device* dev, uint32_t w, uint32_t h, char* why, size_t cap);
    bool ensure_obj(ID3D11Device* dev);
    ID3D11ComputeShader* csObjTile_ = nullptr;   // the search (cs_objsearch)
    ID3D11ComputeShader* csObjPre_ = nullptr;    // the early exits and the list
    ID3D11Buffer* list_ = nullptr;
    ID3D11UnorderedAccessView* listUav_ = nullptr;
    ID3D11ShaderResourceView* listSrv_ = nullptr;
    ID3D11Buffer* listArgs_ = nullptr;
    ID3D11PixelShader* psObjFix_ = nullptr;
    ID3D11Buffer* cbObj_ = nullptr;
    ID3D11Texture2D* tiles_ = nullptr;
    ID3D11UnorderedAccessView* tilesUav_ = nullptr;
    ID3D11ShaderResourceView* tilesSrv_ = nullptr;
    ID3D11Texture2D* tilePrev_[2] = {};
    ID3D11ShaderResourceView* tilePrevSrv_[2] = {};
    bool tilePrevOk_[2] = {};
    ID3D11Texture2D* motion2_ = nullptr;
    ID3D11RenderTargetView* motion2Rtv_ = nullptr;
    uint32_t tilesW_ = 0, tilesH_ = 0, obj2W_ = 0, obj2H_ = 0;
    bool ready_ = false;
    ID3D11VertexShader* vs_ = nullptr;
    ID3D11PixelShader* ps_ = nullptr;
    ID3D11PixelShader* psMask_ = nullptr;
    ID3D11PixelShader* psAudit_ = nullptr;
    ID3D11Buffer* cb_ = nullptr;
    ID3D11RasterizerState* raster_ = nullptr;
    ID3D11BlendState* blend_ = nullptr;
    ID3D11DepthStencilState* ds_ = nullptr;
    ID3D11SamplerState* linear_ = nullptr;
    ID3D11Texture2D* depth_ = nullptr;
    ID3D11Texture2D* motion_ = nullptr;
    ID3D11Texture2D* bias_ = nullptr;
    ID3D11RenderTargetView* depthRtv_ = nullptr;
    ID3D11RenderTargetView* motionRtv_ = nullptr;
    ID3D11RenderTargetView* biasRtv_ = nullptr;
    ID3D11ShaderResourceView* depthSrv_ = nullptr;
    ID3D11ShaderResourceView* motionSrv_ = nullptr;
    ID3D11ShaderResourceView* biasSrv_ = nullptr;
    ID3D11Texture2D* prev_[2] = {};
    ID3D11ShaderResourceView* prevSrv_[2] = {};
    bool prevOk_[2] = {};
    ID3D11Texture2D* auditTex_ = nullptr;
    ID3D11RenderTargetView* auditRtv_ = nullptr;
    ID3D11Texture2D* auditStage_[2] = {};
    ID3D11PixelShader* psFlow_ = nullptr;
    ID3D11Texture2D* flowTex_[2] = {};
    ID3D11RenderTargetView* flowRtv_[2] = {};
    ID3D11Texture2D* flowStage_[2][2] = {};
    bool auditPending_[2] = {};
    float auditExp_[2][2] = {};
    int auditNext_ = 0;
    uint32_t w_ = 0, h_ = 0;
    uint64_t prevBytes_ = 0;
};

} // namespace dvr::dlss
