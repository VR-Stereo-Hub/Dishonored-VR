// Shared by the present-side dump path and its standalone D3D11 host test.
// Requires g_dev11, g_ctx11 and Log; no game or OpenXR dependency.
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
    bool ok=fwrite(fh,1,14,f)==14 && fwrite(ih,1,40,f)==40;
    for(uint32_t y=0;ok&&y<h;++y)ok=fwrite(pixels+(size_t)y*pitch,1,rowB,f)==rowB;
    if(fclose(f)!=0)ok=false;
    return ok;
}

// The worker's job: the pixels (tight rows), their shape, the file.
struct DumpPngJob {
    char     path[MAX_PATH];
    uint8_t* pixels;
    uint32_t w, h;
    bool     bgra;      // the capture is B8G8R8A8/X8; the eye targets are R8G8B8A8
    bool     bitmap;    // burst: fast uncompressed output, no PNG encoding during a timing test
};
static volatile LONG g_dumpPngPending = 0;
static std::atomic<size_t> g_dumpBytes{0};
static std::atomic<unsigned> g_dumpFailures{0};

// Worker thread: WIC encode + write, then one log line with the cost.
static unsigned __stdcall DumpPngThread(void* arg)
{
    DumpPngJob* job = (DumpPngJob*)arg;
    const DWORD t0 = GetTickCount();
    bool ok = false;
    if(job->bitmap){
        if(!job->bgra)for(size_t i=0,n=(size_t)job->w*job->h;i<n;++i)std::swap(job->pixels[i*4],job->pixels[i*4+2]);
        ok=DumpWriteBmp(job->path,job->pixels,job->w,job->h,job->w*4);
    }else {
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
    }
    if(!ok)++g_dumpFailures;
    g_dumpBytes.fetch_sub((size_t)job->w*job->h*4);
    Log("dump: eye %s %s (%lu ms on the dump thread, %u still queued)", job->path, ok ? "written" : "FAILED to write",
        (unsigned long)(GetTickCount() - t0), (unsigned)(InterlockedDecrement(&g_dumpPngPending)));
    free(job->pixels);
    free(job);
    return 0;
}

// A D3D11 texture -> PNG through WIC (already linked for the hand skins): the
// present thread copies the pixels out, a worker thread encodes them.
// alphaOnly (VR-119): the A channel as a grey image, so a dump can say whether
// a stroke's coverage is in the alpha or only in the colour.
static bool DumpTexturePng(const char* path, ID3D11Texture2D* tex, bool alphaOnly = false, bool bitmap = false)
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
            if (alphaOnly) {
                for (size_t i = 0, n = (size_t)d.Width * d.Height; i < n; ++i) {
                    uint8_t* p = px + i * 4;
                    p[0] = p[1] = p[2] = p[3];   // grey = alpha, whatever the channel order
                    p[3] = 255;
                }
            }
            strncpy(job->path, path, MAX_PATH - 1);
            job->pixels = px; job->w = d.Width; job->h = d.Height;job->bitmap=bitmap;
            g_dumpBytes.fetch_add((size_t)d.Width*d.Height*4);
            job->bgra = d.Format == DXGI_FORMAT_B8G8R8A8_UNORM || d.Format == DXGI_FORMAT_B8G8R8X8_UNORM ||
                        d.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || d.Format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
            InterlockedIncrement(&g_dumpPngPending);
            HANDLE th = (HANDLE)_beginthreadex(NULL, 0, DumpPngThread, job, 0, NULL);
            if (th) { CloseHandle(th); ok = true; }
            else { InterlockedDecrement(&g_dumpPngPending);g_dumpBytes.fetch_sub((size_t)d.Width*d.Height*4); free(px); free(job); }
        } else { free(px); free(job); }
        g_ctx11->Unmap(st, 0);
    }
    st->Release();
    return ok;
}
