// AFW replay (VR-39): runs the production held-eye rebuild (src/core/gfx/afw_warp.cpp) on a capture
// written by `afw dump` / the F10 button, and scores it against the NEXT present's native image of the
// same eye. Never touches the game; reads only the local capture folder.
// Build and run: tools\afw-replay.ps1 -Capture <dumps\afw-...> [-Tint] [-Words "stereo off"]
//
// Per present p (1..n-2): the held eye's image and depth come from p's "held" files, the fresh eye's from
// p's "fresh" files, both records from p's header. The rebuild is written to <out>\pNN_replay.raw (RGBA8)
// and compared with p+1's fresh image (the native render of the rebuilt eye, one present later: the
// frame the rebuild alternates with in the headset). The score splits the image by the native depth into
// the near band (< 0.3 units) and the world, and counts pixels whose luminance differs by more than 40.
// The capture is game output: it and every file this writes stay local, never committed.
#include "core/gfx/afw_warp.h"

#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <map>
#include <string>
#include <vector>
#include <initializer_list>

#include "core/util/log.h"
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT] = {};
void write(Cat, Level, const char*, ...) {}
}
static std::map<uint32_t, ID3D11ShaderResourceView*> g_depth;
static float g_fgArg = 0; static bool g_fgOn = true; static float g_nearMissArg = 6.0f; static float g_ownArg = 0.0f;
static UINT g_dw = 0, g_dh = 0;
namespace dvr::clarity { float depth_scale() { return 250.0f; } }
namespace dvr::depthprobe {
ID3D11ShaderResourceView* depth_srv_for(uint32_t serial, UINT* w, UINT* h) {
    auto it = g_depth.find(serial);
    if (it == g_depth.end()) return nullptr;
    if (w) *w = g_dw; if (h) *h = g_dh; return it->second;
}
void read_done(ID3D11DeviceContext*) {}
void set_prefg_wanted(unsigned, bool) {}
bool g_prefgReady = false;
bool prefg_ready() { return g_prefgReady; }   // a capture with signed (masked) depths replays in mask mode
ID3D11ShaderResourceView* prefg_srv_for(uint32_t, bool* saw) { if (saw) *saw = false; return nullptr; }
// Run 17: a capture's depths already carry the drawn mask in their sign; the replay serves none of its own.
ID3D11ShaderResourceView* fgmask_srv_for(uint32_t, uint32_t* draws, uint32_t*, uint32_t*) { if (draws) *draws = 0; return nullptr; }
}

typedef std::map<std::string, std::string> Meta;
static bool load_meta(const char* path, Meta& m) {
    FILE* f = nullptr; if (fopen_s(&f, path, "r") || !f) return false;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        char* e = strchr(line, '='); if (!e) continue;
        *e = 0; std::string v = e + 1; while (!v.empty() && (v.back() == '\n' || v.back() == '\r')) v.pop_back();
        m[line] = v;
    }
    fclose(f); return true;
}
static dvr::afw::Pose pose_of(const std::string& s) {
    dvr::afw::Pose p{}; sscanf_s(s.c_str(), "%f %f %f %f | %f %f %f", &p.q[0], &p.q[1], &p.q[2], &p.q[3], &p.p[0], &p.p[1], &p.p[2]); return p;
}
static bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = nullptr; if (fopen_s(&f, path.c_str(), "rb") || !f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize(n); size_t r = fread(out.data(), 1, n, f); fclose(f); return r == (size_t)n;
}
static float h2f(uint16_t h) {
    const uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
    float v = e == 0 ? ldexpf((float)m, -24) : e == 31 ? INFINITY : ldexpf((float)(m | 1024), (int)e - 25);
    return s ? -v : v;
}

struct Gpu { ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr; };
static ID3D11Texture2D* tex(Gpu& g, UINT w, UINT h, DXGI_FORMAT f, UINT bind, D3D11_USAGE use, UINT cpu, const void* init, UINT pitch) {
    D3D11_TEXTURE2D_DESC td = {}; td.Width = w; td.Height = h; td.MipLevels = td.ArraySize = 1; td.Format = f;
    td.SampleDesc.Count = 1; td.Usage = use; td.BindFlags = bind; td.CPUAccessFlags = cpu;
    D3D11_SUBRESOURCE_DATA sd = {init, pitch, 0};
    ID3D11Texture2D* t = nullptr; g.dev->CreateTexture2D(&td, init ? &sd : nullptr, &t); return t;
}
// A dumped R16F depth as the RGBA16F scene target the production path reads (depth in alpha).
static ID3D11ShaderResourceView* depth_srv(Gpu& g, const std::vector<uint8_t>& raw, UINT w, UINT h) {
    std::vector<uint16_t> rgba((size_t)w * h * 4, 0);
    const uint16_t* d = (const uint16_t*)raw.data();
    for (size_t i = 0; i < (size_t)w * h; ++i) rgba[i * 4 + 3] = d[i];
    ID3D11Texture2D* t = tex(g, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, rgba.data(), w * 8);
    ID3D11ShaderResourceView* s = nullptr; if (t) { g.dev->CreateShaderResourceView(t, nullptr, &s); t->Release(); }
    return s;
}
static void vec(const std::string& s, float* out, int n) {
    const char* p = s.c_str(); for (int i = 0; i < n; ++i) { out[i] = strtof(p, (char**)&p); }
}

static int g_maskArg = 1;
int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: afw-replay <capture dir> <out dir> [debug 0|1] [stereo 0|1] [matrices 0|1]\n"); return 2; }
    setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string dir = argv[1], out = argv[2];
    const bool dbg = argc > 3 && atoi(argv[3]), stereo = argc > 4 ? atoi(argv[4]) != 0 : true, mtx = argc > 5 ? atoi(argv[5]) != 0 : true;
    g_fgArg = argc > 6 ? strtof(argv[6], nullptr) : 0.0f;   // deg; 0 = the world's projection for everything
    g_fgOn = g_fgArg > 0.0f;
    g_nearMissArg = argc > 7 ? strtof(argv[7], nullptr) : 6.0f;
    g_ownArg = argc > 8 ? strtof(argv[8], nullptr) : 0.0f;
    g_maskArg = argc > 9 ? atoi(argv[9]) : 1;   // 0: replay with the foreground mask off (the depth limit)
    CreateDirectoryA(out.c_str(), nullptr);
    Gpu g;
    const D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_0};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 1, D3D11_SDK_VERSION, &g.dev, nullptr, &g.ctx))) { printf("no device\n"); return 2; }
    int n = 0; while (true) { char p[512]; snprintf(p, sizeof(p), "%s\\p%02d.txt", dir.c_str(), n); if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) break; ++n; }
    printf("capture %s: %d presents\n", dir.c_str(), n);
    double sumNear = 0, sumWorld = 0, sumDots = 0; int scored = 0;
    for (int p = 0; p + 1 < n; ++p) {
        char buf[512];
        Meta m, mn;
        snprintf(buf, sizeof(buf), "%s\\p%02d.txt", dir.c_str(), p); if (!load_meta(buf, m)) continue;
        snprintf(buf, sizeof(buf), "%s\\p%02d.txt", dir.c_str(), p + 1); if (!load_meta(buf, mn)) continue;
        if (m["haveHeld"] != "1") continue;
        UINT w = 0, h = 0; if (sscanf_s(m["targetSize"].c_str(), "%ux%u", &w, &h) != 2) sscanf_s(m["freshColor"].c_str(), "ok %ux%u", &w, &h);   // the header's own size (its "target" key is reused for the pose)
        const int held = atoi(m["held"].c_str()), fresh = 1 - held;
        std::vector<uint8_t> hc, fc, hd, fd, next, nextDepth;
        auto path = [&](int q, const char* s) { char b[512]; snprintf(b, sizeof(b), "%s\\p%02d_%s.raw", dir.c_str(), q, s); return std::string(b); };
        if (!read_file(path(p, "held"), hc) || !read_file(path(p, "fresh"), fc) || !read_file(path(p, "held_depth"), hd) ||
            !read_file(path(p, "fresh_depth"), fd) || !read_file(path(p + 1, "fresh"), next) || !read_file(path(p + 1, "fresh_depth"), nextDepth)) {
            printf("p%02d: files missing\n", p); continue;
        }
        if (atoi(mn["fresh"].c_str()) != held) { printf("p%02d: the next present's fresh eye is not this held eye\n", p); continue; }
        UINT dw = w, dh = h; sscanf_s(m["freshDepth"].c_str(), "ok %ux%u", &dw, &dh);   // render size under DLSS SR
        g_dw = dw; g_dh = dh;
        for (auto& kv : g_depth) kv.second->Release();
        g_depth.clear();
        const uint32_t sh = (uint32_t)strtoul(m["heldrec.serial"].c_str(), nullptr, 10), sf = (uint32_t)strtoul(m["fresh.serial"].c_str(), nullptr, 10);
        g_depth[sh] = depth_srv(g, hd, dw, dh); g_depth[sf] = depth_srv(g, fd, dw, dh);
        ID3D11Texture2D* ht = tex(g, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, hc.data(), w * 4);
        ID3D11Texture2D* ft = tex(g, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, fc.data(), w * 4);
        ID3D11Texture2D* dst = tex(g, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, nullptr, 0);
        ID3D11Texture2D* st = tex(g, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, nullptr, 0);
        dvr::afw::set_enabled(true, "replay");
        { char value[8]={};
          const bool freshWorld=GetEnvironmentVariableA("DVR_AFW_FRESHWORLD",value,sizeof(value)) ? value[0]=='1' : atoi(m["freshWorld"].c_str())!=0;
          dvr::afw::set_fresh_world(freshWorld,"replay"); }

        { char value[8]={};
          const bool on=GetEnvironmentVariableA("DVR_AFW_DEPTH_MOTION",value,sizeof(value)) ? value[0]=='1' : atoi(m["depthMotion"].c_str())!=0;
          dvr::afw::set_depth_motion(on,"replay"); }
        dvr::afw::set_debug(dbg, "replay"); dvr::afw::set_stereo(stereo, "replay"); dvr::afw::set_matrices(mtx, "replay");
        dvr::afw::set_world_scale(strtof(m["worldScale"].c_str(), nullptr));
        dvr::afw::set_body_depth(strtof(m["bodyUnits"].c_str(), nullptr), "replay");
        // The foreground FOV: the capture's own, or (older captures) the -Fg argument.
        dvr::afw::set_fg_fov(g_fgArg > 0.0f ? g_fgArg : (m.count("fgFov") ? strtof(m["fgFov"].c_str(), nullptr) : 0.0f));   // the argument overrides the recording (an A/B of the arms lens)
        dvr::afw::set_fg(g_fgOn, "replay");
        dvr::afw::set_near_miss(g_nearMissArg, "replay");
        dvr::depthprobe::g_prefgReady = m["freshMaskOk"] == "1" && g_maskArg != 0;   // the dumped depths carry the mask in their sign
        dvr::afw::set_own_hands(g_ownArg, "replay");
        {   // run 23: the controllers still or moving (DVR_AFW_STILL=0|1; a capture does not record them)
            char e[8] = "";
            dvr::afw::note_hands_still(GetEnvironmentVariableA("DVR_AFW_STILL", e, sizeof(e)) && e[0] == '1');
        }
        {   // run 22: the edge hands under test (DVR_AFW_EDGE=0|1)
            char e[8] = "";
            dvr::afw::set_edge_hands(!(GetEnvironmentVariableA("DVR_AFW_EDGE", e, sizeof(e)) && e[0] == '0'), "replay");
        }
        {   // run 18: the stale tolerance under test (DVR_AFW_STALE=<relative>)
            char e[32] = "";
            if (GetEnvironmentVariableA("DVR_AFW_STALE", e, sizeof(e))) dvr::afw::set_stale(strtof(e, nullptr), "replay");
        }
        // Run 15: the clean images the rebuild compared, when the capture saved them (DVR_AFW_CLEAN=0 ignores them).
        std::vector<uint8_t> hcc, fcc;
        char ce[8] = "";
        const bool useClean = !(GetEnvironmentVariableA("DVR_AFW_CLEAN", ce, sizeof(ce)) && ce[0] == '0');
        // A clean image of another size (run 26: a stale one after a render-size change) is ignored, never read past its end.
        const bool haveHc = useClean && read_file(path(p, "held_clean"), hcc) && hcc.size() == (size_t)w * h * 4;
        const bool haveFc = useClean && read_file(path(p, "fresh_clean"), fcc) && fcc.size() == (size_t)w * h * 4;
        ID3D11Texture2D* hct = haveHc ? tex(g, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, hcc.data(), w * 4) : nullptr;
        ID3D11Texture2D* fct = haveFc ? tex(g, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, fcc.data(), w * 4) : nullptr;
        for (int k = 0; k < 2; ++k) {
            const char* who = k == 0 ? "heldrec" : "fresh";
            auto key = [&](const char* s) { return m[std::string(who) + "." + s]; };
            dvr::afw::Pose tg[2] = {pose_of(key("target0")), pose_of(key("target1"))};
            float vp[16], c5[3], rot[3]; vec(key("vp"), vp, 16); vec(key("c5"), c5, 3); vec(key("rot"), rot, 3);
            dvr::afw::CaptureMeta cm; cm.recId = (uint32_t)atoi(key("rec").c_str());
            if (ID3D11Texture2D* ct = k == 0 ? hct : fct) dvr::afw::note_clean(g.dev, g.ctx, ct, k == 0 ? sh : sf);
            dvr::afw::note_capture(g.dev, g.ctx, k == 0 ? held : fresh, k == 0 ? ht : ft, k == 0 ? sh : sf, pose_of(key("pose")),
                                   key("bodyOk") == "1", strtof(key("bodyYaw").c_str(), nullptr), tg,
                                   key("vpOk") == "1" ? vp : nullptr, key("vpOk") == "1" ? c5 : nullptr,
                                   key("rotOk") == "1" ? rot : nullptr, &cm);
            // Run 25: the grips the image was drawn with, when the capture recorded them (older captures: none).
            dvr::afw::HandPose hp[2];
            for (int h = 0; h < 2; ++h) {
                float v8[8] = {};
                const std::string hk = key(h ? "hand1" : "hand0");
                if (!hk.empty()) { vec(hk, v8, 8); hp[h].ok = v8[0] > 0.5f; for (int i = 0; i < 3; ++i) hp[h].p[i] = v8[1 + i]; for (int i = 0; i < 4; ++i) hp[h].q[i] = v8[4 + i]; }
            }
            {   char e[8] = "";   // DVR_AFW_HELDHANDS=0|1: the controller-moved held hands under test
                if (GetEnvironmentVariableA("DVR_AFW_HELDHANDS", e, sizeof(e))) dvr::afw::set_held_hands(e[0] == '1', "replay"); }
            dvr::afw::note_hands(k == 0 ? held : fresh, hp);
        }
        dvr::afw::Pose outPose{}; const char* why = nullptr;
        const bool ok = dvr::afw::warp_held(g.dev, g.ctx, held, fresh, sf, dst, w, h, strtof(m["tanH"].c_str(), nullptr),
                                            strtof(m["tanV"].c_str(), nullptr), &outPose, &why);
        if (!ok) { printf("p%02d: refused (%s)\n", p, why ? why : "?"); }
        else {
            g.ctx->CopyResource(st, dst);
            D3D11_MAPPED_SUBRESOURCE mp;
            std::vector<uint8_t> px((size_t)w * h * 4);
            if (SUCCEEDED(g.ctx->Map(st, 0, D3D11_MAP_READ, 0, &mp))) {
                for (UINT y = 0; y < h; ++y) memcpy(&px[(size_t)y * w * 4], (uint8_t*)mp.pData + (size_t)y * mp.RowPitch, w * 4);
                g.ctx->Unmap(st, 0);
            }
            FILE* f = nullptr; if (!fopen_s(&f, path(p, "replay").replace(0, dir.size(), out).c_str(), "wb") && f) { fwrite(px.data(), 1, px.size(), f); fclose(f); }
            // Score against the next native frame of this eye.
            const uint16_t* nd = (const uint16_t*)nextDepth.data();
            long nearBad = 0, nearN = 0, worldBad = 0, worldN = 0, dots = 0;
            std::vector<float> lnat((size_t)w * h);
            for (size_t i = 0; i < (size_t)w * h; ++i) lnat[i] = 0.299f * next[i * 4] + 0.587f * next[i * 4 + 1] + 0.114f * next[i * 4 + 2];
            // A dot: a world pixel brighter by 40 than EVERY native pixel of its 3x3 (not a sub-texel edge shift).
            for (UINT y = 1; y + 1 < h; ++y)
                for (UINT x = 1; x + 1 < w; ++x) {
                    const size_t i = (size_t)y * w + x;
                    if (h2f(nd[(size_t)(y * dh / h) * dw + x * dw / w]) < 0.3f) continue;
                    const float la = 0.299f * px[i * 4] + 0.587f * px[i * 4 + 1] + 0.114f * px[i * 4 + 2];
                    float mx = 0;
                    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) mx = fmaxf(mx, lnat[i + dy * (long)w + dx]);
                    dots += la > mx + 40.0f;
                }
            for (size_t i = 0; i < (size_t)w * h; ++i) {
                const float la = 0.299f * px[i * 4] + 0.587f * px[i * 4 + 1] + 0.114f * px[i * 4 + 2];
                const float lb = 0.299f * next[i * 4] + 0.587f * next[i * 4 + 1] + 0.114f * next[i * 4 + 2];
                const bool bad = fabsf(la - lb) > 40.0f;
                const float z = h2f(nd[(size_t)((i / w) * dh / h) * dw + (i % w) * dw / w]);
                if (z > 0 && z < 0.3f) { ++nearN; nearBad += bad; } else { ++worldN; worldBad += bad; }
            }
            printf("p%02d held eye %d: verdict %d | near band %ld px, %.2f%% differ | world %.3f%% differ | bright dots %ld\n", p, held,
                   dvr::afw::matrix_verdict(), nearN, nearN ? 100.0 * nearBad / nearN : 0.0, worldN ? 100.0 * worldBad / worldN : 0.0, dots);
            sumDots += (double)dots;
            sumNear += nearN ? 100.0 * nearBad / nearN : 0; sumWorld += worldN ? 100.0 * worldBad / worldN : 0; ++scored;
        }
        for (ID3D11Texture2D* t : {ht, ft, dst, st, hct, fct}) if (t) t->Release();
    }
    if (scored) printf("MEAN over %d rebuilds: near band %.2f%% differ, world %.3f%% differ (luminance > 40 against the next native frame), bright dots %.0f\n",
                       scored, sumNear / scored, sumWorld / scored, sumDots / scored);
    dvr::afw::shutdown();
    return 0;
}
