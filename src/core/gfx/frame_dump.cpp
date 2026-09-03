// core/gfx/frame_dump.cpp - on-demand dumps of what the headset is being fed.
// Included by the unity build (it reads the renderer's globals).
//
//   dump frame     capture BMP + the stereo method's output texture as PNG
//   dump capture   the game's backbuffer as we captured it (BMP)
//   dump eyes      the stereo method's output texture for the last present
//                  (PNG, named by its eye tag: left|right|mono)
// Files land in <data_dir>\dumps\ with a frame-number suffix.

static int      g_dumpReqCapture = 0;
static int      g_dumpReqEyes = 0;

static void FrameDumpRequest(const char* what)
{
    bool all = !strcmp(what, "frame");
    if (all || !strcmp(what, "capture")) g_dumpReqCapture = 1;
    if (all || !strcmp(what, "eyes"))    g_dumpReqEyes = 1;
    if (!(all || !strcmp(what, "capture") || !strcmp(what, "eyes")))
        Log("dump: unknown target '%s' (frame|capture|eyes)", what);
    else
        Log("dump: %s requested -> %s", what, dvr::paths::dumps_dir());
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

// A D3D11 texture -> PNG through WIC (already linked for the hand skins).
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
        IWICImagingFactory* fac = NULL;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&fac));
        if (SUCCEEDED(hr) && fac) {
            IWICStream* stream = NULL; IWICBitmapEncoder* enc = NULL; IWICBitmapFrameEncode* frame = NULL;
            wchar_t wpath[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
            if (SUCCEEDED(fac->CreateStream(&stream)) &&
                SUCCEEDED(stream->InitializeFromFilename(wpath, GENERIC_WRITE)) &&
                SUCCEEDED(fac->CreateEncoder(GUID_ContainerFormatPng, NULL, &enc)) &&
                SUCCEEDED(enc->Initialize(stream, WICBitmapEncoderNoCache)) &&
                SUCCEEDED(enc->CreateNewFrame(&frame, NULL)) &&
                SUCCEEDED(frame->Initialize(NULL)) &&
                SUCCEEDED(frame->SetSize(d.Width, d.Height))) {
                // the eye targets are R8G8B8A8; the capture is B8G8R8A8/X8
                WICPixelFormatGUID fmt = (d.Format == DXGI_FORMAT_B8G8R8A8_UNORM || d.Format == DXGI_FORMAT_B8G8R8X8_UNORM)
                                             ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat32bppRGBA;
                if (SUCCEEDED(frame->SetPixelFormat(&fmt)) &&
                    SUCCEEDED(frame->WritePixels(d.Height, m.RowPitch, m.RowPitch * d.Height, (BYTE*)m.pData)) &&
                    SUCCEEDED(frame->Commit()) && SUCCEEDED(enc->Commit()))
                    ok = true;
            }
            if (frame) frame->Release();
            if (enc) enc->Release();
            if (stream) stream->Release();
            fac->Release();
        }
        g_ctx11->Unmap(st, 0);
    }
    st->Release();
    return ok;
}

// Present thread, after the eyes are rendered and before they are submitted.
static void FrameDumpTick(IDirect3DDevice9* dev)
{
    if (!g_dumpReqCapture && !g_dumpReqEyes) return;
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
    if (g_dumpReqEyes) {
        g_dumpReqEyes = 0;
        const dvr::stereo::FrameOutput& o = dvr::stereo::last_output();
        snprintf(path, MAX_PATH, "%s\\eye_%lu_%s.png", dvr::paths::dumps_dir(), (unsigned long)g_frame,
                 o.eyeSign < 0 ? "left" : o.eyeSign > 0 ? "right" : "mono");
        Log("dump: eye %s", DumpTexturePng(path, o.tex) ? path : "FAILED (no output texture this present?)");
    }
}
