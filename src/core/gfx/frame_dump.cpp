// core/gfx/frame_dump.cpp - on-demand dumps of what the headset is being fed.
// Included by the unity build (it reads the renderer's globals).
//
//   dump frame     capture BMP + the stereo method's output texture as PNG
//   dump capture   the game's backbuffer as we captured it (BMP)
//   dump eyes      the stereo method's output texture for the next TWO
//                  presents (PNG each, named by eye tag: left|right|mono), so
//                  one request yields a consecutive pair under a two-presents-
//                  per-tick method - the picture that says whether the two
//                  eyes carry the same scene (41.1, session 8)
// Files land in <data_dir>\dumps\ with a frame-number suffix.
//
// 41.1 (session 9): the PNG is encoded on a worker thread. Encoding a 27 MB
// eye texture on the present thread took 620-660 ms per file (headset run 17),
// long enough for the script camera writes to read stale, the state to drop
// to LOADING and the second draw to re-arm - so every `dump eyes` used to
// re-arm the very doubling the dump was taken to judge. The present thread
// now only copies the staging texture into a heap buffer (a few ms).

#include <process.h>   // _beginthreadex (the dump thread)
#include "core/gfx/frame_burst.h"

static int      g_dumpReqCapture = 0;
static int      g_dumpReqEyes = 0;
static int      g_dumpReqHud = -1;    // VR-117: `dump hud [sink]` (-1 = none)

static void FrameDumpRequest(const char* what)
{
    bool all = !strcmp(what, "frame");
    if (all || !strcmp(what, "capture")) g_dumpReqCapture = 1;
    if (all || !strcmp(what, "eyes"))    g_dumpReqEyes = 2;   // a consecutive pair
    if (!strncmp(what, "hud", 3)) {   // VR-117: a sink's delivered texture (default sink 0)
        const char* a = what + 3;
        while (*a == ' ') ++a;
        g_dumpReqHud = *a ? atoi(a) : 0;
        Log("dump: hud sink %d requested (geometry only: DumpTexturePng swaps R and B, VR-13)", g_dumpReqHud);
        return;
    }
    if (!(all || !strcmp(what, "capture") || !strcmp(what, "eyes")))
        Log("dump: unknown target '%s' (frame|capture|eyes|hud [sink])", what);
    else
        Log("dump: %s requested -> %s", what, dvr::paths::dumps_dir());
}

#include "core/gfx/frame_dump_io.inc"

// The burst consumes last_output at the same point as the established eye
// dump. That image belongs to the PREVIOUS present, with its delivered record.
// Queueing uses bounded CPU memory; backpressure is counted, never called
// consecutive capture. Readback may perturb cadence, even with disk I/O off-lane.
static dvr::capture::Burst g_frameBurst;
static char g_frameBurstDir[MAX_PATH]{};
static unsigned g_frameBurstFailures=0;
static bool g_frameBurstAfw=false;
static void FrameBurstRequest()
{
    if(g_frameBurst.busy()||g_dumpPngPending)return;
    if(!_stricmp(dvr::stereo::active_name(),"afw")){
        g_frameBurstAfw=true;
        dvr::afw::request_dump(16,5000,dvr::paths::dumps_dir(),"F10 frame burst");return;
    }
    g_frameBurstAfw=false;
    if(!g_frameBurst.start(GetTickCount64()))return;
    SYSTEMTIME t;GetLocalTime(&t);
    _snprintf_s(g_frameBurstDir,sizeof(g_frameBurstDir),_TRUNCATE,"%s\\frames-%04u%02u%02u-%02u%02u%02u-%03u",
        dvr::paths::dumps_dir(),t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
    if(!CreateDirectoryA(g_frameBurstDir,nullptr)){g_frameBurst.phase=dvr::capture::Burst::Failed;Log("dump/burst: cannot create %s (error %lu)",g_frameBurstDir,GetLastError());return;}
    g_frameBurstFailures=g_dumpFailures.load();
    Log("dump/burst: armed 16 native eye images after 5 seconds, method=%s, directory=%s; raw BMP workers, max 96 MiB queued, readback can perturb cadence; gaps are recorded",
        dvr::stereo::active_name(),g_frameBurstDir);
}
static const char* FrameBurstStatus()
{
    if(g_frameBurstAfw)return dvr::afw::dump_status();
    static char text[192];
    const auto& b=g_frameBurst;
    const auto now=GetTickCount64();
    switch(b.phase){
    case dvr::capture::Burst::Armed:
        _snprintf_s(text,sizeof(text),_TRUNCATE,"Starts in %.1f seconds - close the menu and reproduce the issue",b.due>now?(b.due-now)*.001:0.);break;
    case dvr::capture::Burst::Capturing:case dvr::capture::Burst::Saving:
        _snprintf_s(text,sizeof(text),_TRUNCATE,"Captured %u / %u; saving images (%u busy frames skipped)",b.captured,b.wanted,b.skipped);break;
    case dvr::capture::Burst::Done:_snprintf_s(text,sizeof(text),_TRUNCATE,"Saved %u images. Ready for analysis.",b.captured);break;
    case dvr::capture::Burst::Failed:strcpy_s(text,"Capture incomplete. See the log for saved frames and failures.");break;
    default:strcpy_s(text,"No capture yet");break;
    }
    return text;
}
static void FrameBurstTick()
{
    if(!g_frameBurst.busy())return;
    const auto previous=g_frameBurst.phase;
    g_frameBurst.saved(g_dumpPngPending!=0,g_dumpFailures.load()!=g_frameBurstFailures);
    const auto& o=dvr::stereo::last_output();
    const uint32_t serial=dvr::capture::delivered_serial();
    D3D11_TEXTURE2D_DESC desc{};if(o.tex)o.tex->GetDesc(&desc);
    const size_t bytes=(size_t)desc.Width*desc.Height*4;
    constexpr size_t budget=96u*1024u*1024u;
    const bool format=desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM||desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||
        desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM||desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const bool capacity=bytes<=budget&&g_dumpBytes.load()<=budget-bytes&&g_dumpPngPending<3;
    if(o.tex&&(!format||desc.SampleDesc.Count!=1||bytes>budget)){
        g_frameBurst.phase=dvr::capture::Burst::Failed;
        Log("dump/burst: unsupported image %ux%u format=%u samples=%u bytes=%zu (budget=%zu)",
            desc.Width,desc.Height,(unsigned)desc.Format,desc.SampleDesc.Count,bytes,budget);
    }
    if(g_frameBurst.ready(GetTickCount64(),serial,o.tex&&format&&desc.SampleDesc.Count==1,capacity)){
        char path[MAX_PATH];
        _snprintf_s(path,sizeof(path),_TRUNCATE,"%s\\f%02u_p%lu_s%u_%s.bmp",g_frameBurstDir,g_frameBurst.captured,
            (unsigned long)(g_frame-1),serial,o.eyeSign<0?"left":o.eyeSign>0?"right":"mono");
        bool ok=DumpTexturePng(path,o.tex,false,true);
        dvr::pose::Record rec{};bool have=dvr::pose::copy(dvr::capture::delivered_rec(),&rec);
        Log("dump/burst: image=%u previous-present=%lu serial=%u eye=%d rec=%u pair=%u locate=%u metadata=%d queued=%d busySkipped=%u path=%s",
            g_frameBurst.captured,(unsigned long)(g_frame-1),serial,o.eyeSign,rec.id,rec.pairId,rec.track.gen,have?1:0,ok?1:0,g_frameBurst.skipped,path);
        _snprintf_s(path,sizeof(path),_TRUNCATE,"%s\\frames.csv",g_frameBurstDir);
        FILE* f=nullptr;
        if(!fopen_s(&f,path,g_frameBurst.captured?"a":"w")&&f){
            if(!g_frameBurst.captured)fprintf(f,"image,previousPresent,serial,eye,record,pair,headLocate,metadata,queued,timeMs,busySkipped\n");
            fprintf(f,"%u,%lu,%u,%d,%u,%u,%u,%d,%d,%llu,%u\n",g_frameBurst.captured,(unsigned long)(g_frame-1),serial,o.eyeSign,
                rec.id,rec.pairId,rec.track.gen,have?1:0,ok?1:0,(unsigned long long)GetTickCount64(),g_frameBurst.skipped);
            if(ferror(f))ok=false;
            if(fclose(f)!=0)ok=false;
        }else ok=false;
        g_frameBurst.queued(ok);
    }
    if(previous!=g_frameBurst.phase&&(g_frameBurst.phase==dvr::capture::Burst::Done||g_frameBurst.phase==dvr::capture::Burst::Failed))
        Log("dump/burst: %s; %s",FrameBurstStatus(),g_frameBurstDir);
}

// Present thread, after the eyes are rendered and before they are submitted.
static void FrameDumpTick(IDirect3DDevice9* dev)
{
    FrameBurstTick();
    if (!g_dumpReqCapture && !g_dumpReqEyes && g_dumpReqHud < 0) return;
    char path[MAX_PATH];
    if (g_dumpReqHud >= 0) {
        const int sink = g_dumpReqHud;
        g_dumpReqHud = -1;
        snprintf(path, MAX_PATH, "%s\\hud_s%d_%lu.png", dvr::paths::dumps_dir(), sink, (unsigned long)g_frame);
        ID3D11Texture2D* t = dvr::hudcap::panel_texture(sink);
        Log("dump: hud sink %d %s -> %s", sink,
            !t ? "FAILED (no texture on that sink - is [Hud] Panel on and the sink in use? `hud status`)"
               : DumpTexturePng(path, t) ? "queued (the dump thread writes it)" : "FAILED", path);
        if (t) {   // VR-119: the alpha channel beside it, as grey
            snprintf(path, MAX_PATH, "%s\\hud_s%d_%lu_alpha.png", dvr::paths::dumps_dir(), sink, (unsigned long)g_frame);
            Log("dump: hud sink %d alpha %s -> %s (grey = the quad's alpha; in mode repair it is max(r,g,b), in "
                "captured the sink's own coverage)", sink, DumpTexturePng(path, t, true) ? "queued" : "FAILED", path);
        }
    }
    if (g_dumpReqCapture) {
        g_dumpReqCapture = 0;
        dvr::capture::snapshot_pixels(dev);   // shared mode: read this present back, not the 3 s sample
        const uint8_t* px = dvr::capture::pixels();
        const uint32_t cw = dvr::capture::width(), ch = dvr::capture::height();
        if (px && cw && ch) {
            snprintf(path, MAX_PATH, "%s\\capture_%lu_%ux%u.bmp", dvr::paths::dumps_dir(), (unsigned long)g_frame, cw, ch);
            Log("dump: capture %s", DumpWriteBmp(path, px, cw, ch, cw * 4) ? path : "FAILED");
        } else Log("dump: no capture yet");
    }
    if (g_dumpReqEyes) {
        // The pair is a left THEN its right (one tick's two draws): under a
        // per-eye method the first file waits for a -1 output (headset run 07
        // wrote a right then the next tick's left).
        if (g_dumpReqEyes == 2 && dvr::stereo::last_output().eyeSign > 0) return;
        --g_dumpReqEyes;
        // last_output() is the PREVIOUS present's output (this tick runs before
        // end_frame), so eye_<N> holds present N-1's eye, tagged as delivered.
        const dvr::stereo::FrameOutput& o = dvr::stereo::last_output();
        snprintf(path, MAX_PATH, "%s\\eye_%lu_%s.png", dvr::paths::dumps_dir(), (unsigned long)g_frame,
                 o.eyeSign < 0 ? "left" : o.eyeSign > 0 ? "right" : "mono");
        Log("dump: eye %s -> %s", path, DumpTexturePng(path, o.tex) ? "queued (the dump thread writes it; the present "
                                                                     "thread does not wait)" : "FAILED (no output texture this present?)");
    }

}
