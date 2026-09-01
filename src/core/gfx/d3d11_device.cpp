// core/gfx/d3d11_device.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ----------------------------------------------------------------------------
// D3D11 renderer
// ----------------------------------------------------------------------------
static bool EnsureD3D11()
{
    if (g_dev11) return true;
    if (g_d3d11Failed) return false;
    if (!g_d3d11mod) g_d3d11mod = LoadLibraryA("d3d11.dll");
    if (!g_d3d11mod) { Log("no d3d11.dll"); g_d3d11Failed = true; return false; }
    PFN_D3D11CreateDevice create =
        (PFN_D3D11CreateDevice)GetProcAddress(g_d3d11mod, "D3D11CreateDevice");
    if (!create) { g_d3d11Failed = true; return false; }
    HRESULT hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                        D3D11_SDK_VERSION, &g_dev11, NULL, &g_ctx11);
    if (FAILED(hr) || !g_dev11) {
        Log("D3D11CreateDevice failed (0x%08lx)", (unsigned long)hr);
        g_d3d11Failed = true;
        return false;
    }
    Log("D3D11 device created");
    return true;
}


static bool EnsureGameTex(UINT w, UINT h)
{
    if (g_texGame[0] && g_texW == w && g_texH == h) return true;
    for (int e = 0; e < 2; e++) {
        if (g_srvGame[e]) { g_srvGame[e]->Release(); g_srvGame[e] = NULL; }
        if (g_texGame[e]) { g_texGame[e]->Release(); g_texGame[e] = NULL; }
    }
    D3D11_TEXTURE2D_DESC td;
    memset(&td, 0, sizeof(td));
    td.Width = w; td.Height = h;
    td.MipLevels = 1; td.ArraySize = 1;
    // BGRA matches D3D9's X8R8G8B8 byte order, so we can copy the captured
    // frame straight in with no per-pixel conversion (the sampler still returns
    // correct RGB). This removes the biggest CPU cost per frame.
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    for (int e = 0; e < 2; e++) {
        if (FAILED(g_dev11->CreateTexture2D(&td, NULL, &g_texGame[e])) ||
            FAILED(g_dev11->CreateShaderResourceView(g_texGame[e], NULL, &g_srvGame[e]))) {
            Log("game tex/SRV create failed %ux%u eye %d", w, h, e);
            return false;
        }
    }
    g_texW = w; g_texH = h;
    Log("game frame textures (x2): %ux%u", w, h);
    return true;
}


static bool EnsurePipeline()
{
    if (g_pipelineReady) return true;

    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    if (!compiler) { Log("d3dcompiler_47.dll missing"); return false; }
    PFN_D3DCompile compile = (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile");
    if (!compile) return false;

    ID3DBlob *vsb = NULL, *psb = NULL, *err = NULL;
    if (FAILED(compile(kShaderSrc, strlen(kShaderSrc), NULL, NULL, NULL,
                       "vsmain", "vs_4_0", 0, 0, &vsb, &err))) {
        Log("VS compile failed: %s", err ? (char*)err->GetBufferPointer() : "?");
        return false;
    }
    if (FAILED(compile(kShaderSrc, strlen(kShaderSrc), NULL, NULL, NULL,
                       "psmain", "ps_4_0", 0, 0, &psb, &err))) {
        Log("PS compile failed: %s", err ? (char*)err->GetBufferPointer() : "?");
        return false;
    }
    if (FAILED(g_dev11->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), NULL, &g_vs)) ||
        FAILED(g_dev11->CreatePixelShader (psb->GetBufferPointer(), psb->GetBufferSize(), NULL, &g_ps))) {
        Log("shader object creation failed");
        return false;
    }
    D3D11_INPUT_ELEMENT_DESC il[2] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(g_dev11->CreateInputLayout(il, 2, vsb->GetBufferPointer(),
                                          vsb->GetBufferSize(), &g_layout))) {
        Log("input layout failed");
        return false;
    }
    vsb->Release(); psb->Release();

    D3D11_SAMPLER_DESC smp;
    memset(&smp, 0, sizeof(smp));
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(g_dev11->CreateSamplerState(&smp, &g_sampler))) return false;

    // per-eye render targets at the compositor's recommended size
    uint32_t w = 0, h = 0;
    if (g_xrBackend && g_xrEyeW) { w = g_xrEyeW; h = g_xrEyeH; }   // 37.3
    else if (g_sys) g_sys->GetRecommendedRenderTargetSize(&w, &h);
    if (!w || !h) { w = 1852; h = 2056; } // sane Index default
    for (int eye = 0; eye < 2; eye++) {
        D3D11_TEXTURE2D_DESC td;
        memset(&td, 0, sizeof(td));
        td.Width = w; td.Height = h;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        D3D11_TEXTURE2D_DESC dd = td;
        dd.Format = DXGI_FORMAT_D32_FLOAT;
        dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (SUCCEEDED(g_dev11->CreateTexture2D(&dd, NULL, &g_eyeDS[eye])))
            g_dev11->CreateDepthStencilView(g_eyeDS[eye], NULL, &g_eyeDSV[eye]);
        if (FAILED(g_dev11->CreateTexture2D(&td, NULL, &g_eyeTex[eye])) ||
            FAILED(g_dev11->CreateRenderTargetView(g_eyeTex[eye], NULL, &g_eyeRTV[eye]))) {
            Log("eye RT %d failed (%ux%u)", eye, w, h);
            return false;
        }
    }
    g_eyeW = w; g_eyeH = h;
    if (!g_dsOn) {
        D3D11_DEPTH_STENCIL_DESC ds; memset(&ds, 0, sizeof(ds));
        ds.DepthEnable = FALSE;
        g_dev11->CreateDepthStencilState(&ds, &g_dsOff);
        ds.DepthEnable = TRUE;
        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        ds.DepthFunc = D3D11_COMPARISON_LESS;
        g_dev11->CreateDepthStencilState(&ds, &g_dsOn);
    }
    Log("eye render targets: %ux%u per eye", w, h);
    g_pipelineReady = true;
    return true;
}
