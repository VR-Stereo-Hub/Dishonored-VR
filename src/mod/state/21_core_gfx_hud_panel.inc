// State chunk 21 for core/gfx/hud_panel - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// 38.13: XR RETICLE - the aim crosshair as scene geometry. Under the XR
// backend there is no OpenVR overlay interface (asking for it lazily was
// the arming crash), so ReticleTick publishes the aim point + size here and
// the eye loop draws the same procedural ring as a billboard, per eye.
static bool  g_retXrOn = false;
static float g_retXrPos[3];             // room space
static float g_retXrSize = 0.02f;
static ID3D11Texture2D*          g_retTex11 = NULL;
static ID3D11ShaderResourceView* g_retSrv11 = NULL;
static ID3D11Buffer*             g_retVb11  = NULL;
