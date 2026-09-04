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

static int      g_dumpReqCapture = 0;
static int      g_dumpReqEyes = 0;

static void FrameDumpRequest(const char* what)
{
    bool all = !strcmp(what, "frame");
    if (all || !strcmp(what, "capture")) g_dumpReqCapture = 1;
    if (all || !strcmp(what, "eyes"))    g_dumpReqEyes = 2;   // a consecutive pair
    if (!(all || !strcmp(what, "capture") || !strcmp(what, "eyes")))
        Log("dump: unknown target '%s' (frame|capture|eyes)", what);
    else
        Log("dump: %s requested -> %s", what, dvr::paths::dumps_dir());
    if (strstr(what, "hud")) { g_dumpReqHud = 1; Log("dump: hud requested"); }
}

// Four lines of header and no library: top-down BGRA.
static bool DumpWriteBmp(const char* path, const uint8_t* pixels, uint32_t w, uint32_t h, uint32_t pitch)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    const uint32_t rowB = w * 4, imgB = rowB * h;
    uint8_t fh[14] = { 'B', 'M' }, ih[40] = {};
    uint32_t fsz = 14 + 40 + imgB, off = 54;
    memcpy(fh + 2, &fsz, 4); memcpy(fh + 10, &off, 4);
    uint32_t v40 = 40; memcpy(ih, &v40, 4);
    int32_t iw = (int32_t)w, ih2 = -(int32_t)h;
    memcpy(ih + 4, &iw, 4); memcpy(ih + 8, &ih2, 4);
    uint16_t planes = 1, bpp = 32;
    memcpy(ih + 12, &planes, 2); memcpy(ih + 14, &bpp, 2);
    memcpy(ih + 20, &imgB, 4);
    fwrite(fh, 1, 14, f); fwrite(ih, 1, 40, f);
    for (uint32_t y = 0; y < h; y++) fwrite(pixels + (size_t)y * pitch, 1, rowB, f);
    fclose(f);
    return true;
}

// The worker's job: the pixels (tight rows), their shape, the file.
struct DumpPngJob {
    char     path[MAX_PATH];
    uint8_t* pixels;
    uint32_t w, h;
    bool     bgra;      // the capture is B8G8R8A8/X8; the eye targets are R8G8B8A8
};
static volatile LONG g_dumpPngPending = 0;

// Worker thread: WIC encode + write, then one log line with the cost.
static unsigned __stdcall DumpPngThread(void* arg)
{
    DumpPngJob* job = (DumpPngJob*)arg;
    const DWORD t0 = GetTickCount();
    bool ok = false;
    const HRESULT coHr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IWICImagingFactory* fac = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac));
    if (SUCCEEDED(hr) && fac) {
        IWICStream* stream = NULL; IWICBitmapEncoder* enc = NULL; IWICBitmapFrameEncode* frame = NULL;
        wchar_t wpath[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, job->path, -1, wpath, MAX_PATH);
        if (SUCCEEDED(fac->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromFilename(wpath, GENERIC_WRITE)) &&
            SUCCEEDED(fac->CreateEncoder(GUID_ContainerFormatPng, NULL, &enc)) &&
            SUCCEEDED(enc->Initialize(stream, WICBitmapEncoderNoCache)) &&
            SUCCEEDED(enc->CreateNewFrame(&frame, NULL)) &&
            SUCCEEDED(frame->Initialize(NULL)) &&
            SUCCEEDED(frame->SetSize(job->w, job->h))) {
            WICPixelFormatGUID fmt = job->bgra ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat32bppRGBA;
            if (SUCCEEDED(frame->SetPixelFormat(&fmt)) &&
                SUCCEEDED(frame->WritePixels(job->h, job->w * 4, job->w * 4 * job->h, (BYTE*)job->pixels)) &&
                SUCCEEDED(frame->Commit()) && SUCCEEDED(enc->Commit()))
                ok = true;
        }
        if (frame) frame->Release();
        if (enc) enc->Release();
        if (stream) stream->Release();
        fac->Release();
    }
    if (SUCCEEDED(coHr)) CoUninitialize();
    Log("dump: eye %s %s (%lu ms on the dump thread, %u still queued)", job->path, ok ? "written" : "FAILED to encode",
        (unsigned long)(GetTickCount() - t0), (unsigned)(InterlockedDecrement(&g_dumpPngPending)));
    free(job->pixels);
    free(job);
    return 0;
}

// A D3D11 texture -> PNG through WIC (already linked for the hand skins): the
// present thread copies the pixels out, a worker thread encodes them.
static bool DumpTexturePng(const char* path, ID3D11Texture2D* tex)
{
    if (!g_dev11 || !g_ctx11 || !tex) return false;
    D3D11_TEXTURE2D_DESC d; tex->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.MiscFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* st = NULL;
    if (FAILED(g_dev11->CreateTexture2D(&d, NULL, &st)) || !st) return false;
    g_ctx11->CopyResource(st, tex);
    D3D11_MAPPED_SUBRESOURCE m;
    bool ok = false;
    if (SUCCEEDED(g_ctx11->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
        DumpPngJob* job = (DumpPngJob*)calloc(1, sizeof(DumpPngJob));
        uint8_t* px = (uint8_t*)malloc((size_t)d.Width * d.Height * 4);
        if (job && px) {
            for (uint32_t y = 0; y < d.Height; y++)
                memcpy(px + (size_t)y * d.Width * 4, (const uint8_t*)m.pData + (size_t)y * m.RowPitch, (size_t)d.Width * 4);
            strncpy(job->path, path, MAX_PATH - 1);
            job->pixels = px; job->w = d.Width; job->h = d.Height;
            job->bgra = d.Format == DXGI_FORMAT_B8G8R8A8_UNORM || d.Format == DXGI_FORMAT_B8G8R8X8_UNORM;
            InterlockedIncrement(&g_dumpPngPending);
            HANDLE th = (HANDLE)_beginthreadex(NULL, 0, DumpPngThread, job, 0, NULL);
            if (th) { CloseHandle(th); ok = true; }
            else { InterlockedDecrement(&g_dumpPngPending); free(px); free(job); }
        } else { free(px); free(job); }
        g_ctx11->Unmap(st, 0);
    }
    st->Release();
    return ok;
}

// Present thread, after the eyes are rendered and before they are submitted.
static void FrameDumpTick(IDirect3DDevice9* dev)
{
    if (!g_dumpReqCapture && !g_dumpReqEyes && !g_dumpReqHud) return;
    char path[MAX_PATH];
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
    if (g_dumpReqHud) {
        g_dumpReqHud = 0;
        snprintf(path, MAX_PATH, "%s\\hud_%lu.png", dvr::paths::dumps_dir(), (unsigned long)g_frame);
        ID3D11Texture2D* t = dvr::hudcap::panel_texture();
        Log("dump: hud %s -> %s", path,
            !t ? "FAILED (no panel texture - is [Hud] Panel on?)"
               : DumpTexturePng(path, t) ? "queued (the dump thread writes it)" : "FAILED");
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
