// core/gfx/hud_panel.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ---- 34.7: wrist HUD panel -------------------------------------------------
static bool EnsureHudPanel11()
{
    if (g_hudTex11 && g_hudSrv11 && g_vbHud && g_bsAdd) return true;
    if (!g_dev11) return false;
    if (!g_hudTex11) {
        D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof(td));
        td.Width = HUDRB_W; td.Height = HUDRB_H;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g_dev11->CreateTexture2D(&td, NULL, &g_hudTex11)) ||
            FAILED(g_dev11->CreateShaderResourceView(g_hudTex11, NULL, &g_hudSrv11))) {
            static bool told = false;
            if (!told) { told = true; Log("hud: panel texture create FAILED"); }
            return false;
        }
    }
    if (!g_vbHud) {
        D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof(bd));
        bd.ByteWidth = 4 * sizeof(QuadVert);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(g_dev11->CreateBuffer(&bd, NULL, &g_vbHud))) return false;
    }
    if (!g_bsAdd) {
        D3D11_BLEND_DESC bl; memset(&bl, 0, sizeof(bl));
        bl.RenderTarget[0].BlendEnable    = TRUE;
        bl.RenderTarget[0].SrcBlend       = D3D11_BLEND_ONE;
        bl.RenderTarget[0].DestBlend      = D3D11_BLEND_ONE;
        bl.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
        bl.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
        bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        bl.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(g_dev11->CreateBlendState(&bl, &g_bsAdd))) return false;
    }
    Log("hud: panel pipeline ready (%dx%d source)", HUDRB_W, HUDRB_H);
    return true;
}


// Corners of the wrist billboard in HEAD space (x right, y up, -z forward),
// order TL TR BL BR to match the screen quad's triangle strip + UVs.
static bool HudPanelCorners(float c[4][3])
{
    int dev = g_ctrlIdx[(g_hudPanelHand == 1) ? 1 : 0];
    if (dev < 0 || dev >= 16 || !g_devPoseOk[dev] || !g_devPoseOk[0])
        return false;
    float (*H)[4] = g_devPose[0];
    float (*C)[4] = g_devPose[dev];
    // controller position relative to the head, in head axes: R_h^T (pc - ph)
    float d[3] = { C[0][3] - H[0][3], C[1][3] - H[1][3], C[2][3] - H[2][3] };
    float pr[3];
    for (int k = 0; k < 3; k++)
        pr[k] = H[0][k] * d[0] + H[1][k] * d[1] + H[2][k] * d[2];
    pr[1] += g_hudPanelUp;               // float it just above the controller
    if (pr[2] > -0.06f) return false;    // behind / at the face: hide
    // billboard the panel at the head
    float n[3] = { -pr[0], -pr[1], -pr[2] };
    if (V3Norm(n) < 0.05f) return false;
    float upW[3] = { 0.0f, 1.0f, 0.0f };
    float rgt[3]; V3Cross(upW, n, rgt);
    if (V3Norm(rgt) < 0.2f) return false;   // panel almost overhead: hide
    float upP[3]; V3Cross(n, rgt, upP); V3Norm(upP);
    float hw = g_hudPanelSize * 0.5f;
    float hh = hw * ((float)HUDRB_H / (float)HUDRB_W);
    for (int k = 0; k < 3; k++) {
        c[0][k] = pr[k] - rgt[k] * hw + upP[k] * hh;   // TL
        c[1][k] = pr[k] + rgt[k] * hw + upP[k] * hh;   // TR
        c[2][k] = pr[k] - rgt[k] * hw - upP[k] * hh;   // BL
        c[3][k] = pr[k] + rgt[k] * hw - upP[k] * hh;   // BR
    }
    return true;
}


static void HudPanelDrawEye(int eye, const float c[4][3])
{
    if (!g_eyeFrOk) return;
    float l = g_eyeFr[eye][0], r = g_eyeFr[eye][1];
    float t = g_eyeFr[eye][2], b = g_eyeFr[eye][3];
    float idx = 1.0f / (r - l), idy = 1.0f / (b - t);
    float ex = g_eyeOffs[eye][0], ey = g_eyeOffs[eye][1], ez = g_eyeOffs[eye][2];
    static const float uvs[4][2] = { {0,0}, {1,0}, {0,1}, {1,1} };
    QuadVert v[4];
    const float* R = g_eyeRot[eye];      // 38.1: eye cant (identity on Vive)
    for (int i = 0; i < 4; i++) {
        float hx = c[i][0] - ex, hy = c[i][1] - ey, hz = c[i][2] - ez;
        float x = hx, y = hy, z = hz;
        if (g_eyeCantCfg) {
            x = R[0]*hx + R[3]*hy + R[6]*hz;
            y = R[1]*hx + R[4]*hy + R[7]*hz;
            z = R[2]*hx + R[5]*hy + R[8]*hz;
        }
        v[i].pos[0] = 2*idx*x + (r + l)*idx*z;
        v[i].pos[1] = 2*idy*y + (b + t)*idy*z;
        v[i].pos[3] = -z;
        v[i].pos[2] = 0.5f * v[i].pos[3];    // mid-depth, depth test off
        v[i].uv[0] = uvs[i][0]; v[i].uv[1] = uvs[i][1];
    }
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(g_ctx11->Map(g_vbHud, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
        return;
    memcpy(ms.pData, v, sizeof(v));
    g_ctx11->Unmap(g_vbHud, 0);
    UINT stride = sizeof(QuadVert), offset = 0;
    float bf[4] = { 0, 0, 0, 0 };
    g_ctx11->OMSetDepthStencilState(g_dsOff, 0);
    g_ctx11->OMSetBlendState(g_bsAdd, bf, 0xffffffff);
    g_ctx11->PSSetShaderResources(0, 1, &g_hudSrv11);
    g_ctx11->IASetVertexBuffers(0, 1, &g_vbHud, &stride, &offset);
    g_ctx11->Draw(4, 0);
    g_ctx11->OMSetBlendState(NULL, NULL, 0xffffffff);
}


static bool XrRetEnsure()
{
    if (g_retSrv11 && g_retVb11) return true;
    if (!g_dev11) return false;
    if (!g_retSrv11) {
        static uint32_t px[64 * 64];
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                float dx = x - 31.5f, dy = y - 31.5f;
                float rr = sqrtf(dx*dx + dy*dy);
                float dotA  = 4.5f - rr;
                float ringA = 2.2f - fabsf(rr - 19.0f);
                float w = dotA > ringA ? dotA : ringA;
                if (w < 0) w = 0; if (w > 1) w = 1;
                unsigned lum = (unsigned)(255.0f * w);
                px[y*64 + x] = (lum << 24) | (lum << 16) | (lum << 8) | lum;
            }
        }
        D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof(td));
        td.Width = 64; td.Height = 64; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd; memset(&sd, 0, sizeof(sd));
        sd.pSysMem = px; sd.SysMemPitch = 64 * 4;
        if (FAILED(g_dev11->CreateTexture2D(&td, &sd, &g_retTex11)) ||
            FAILED(g_dev11->CreateShaderResourceView(g_retTex11, NULL,
                                                     &g_retSrv11)))
            return false;
    }
    if (!g_retVb11) {
        D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof(bd));
        bd.ByteWidth = 4 * sizeof(QuadVert);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(g_dev11->CreateBuffer(&bd, NULL, &g_retVb11))) return false;
    }
    if (!g_bsAdd) {   // the wrist panel usually made this; don't depend on it
        D3D11_BLEND_DESC bl; memset(&bl, 0, sizeof(bl));
        bl.RenderTarget[0].BlendEnable    = TRUE;
        bl.RenderTarget[0].SrcBlend       = D3D11_BLEND_ONE;
        bl.RenderTarget[0].DestBlend      = D3D11_BLEND_ONE;
        bl.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
        bl.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
        bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        bl.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(g_dev11->CreateBlendState(&bl, &g_bsAdd))) return false;
    }
    return true;
}


// billboard corners at the head, same shape as the wrist panel's
static bool XrRetCorners(float c[4][3])
{
    if (!g_devPoseOk[0]) return false;
    float (*H)[4] = g_devPose[0];
    float d[3] = { g_retXrPos[0] - H[0][3], g_retXrPos[1] - H[1][3],
                   g_retXrPos[2] - H[2][3] };
    float pr[3];
    for (int k = 0; k < 3; k++)
        pr[k] = H[0][k] * d[0] + H[1][k] * d[1] + H[2][k] * d[2];
    if (pr[2] > -0.06f) return false;
    float n[3] = { -pr[0], -pr[1], -pr[2] };
    if (V3Norm(n) < 0.05f) return false;
    float upW[3] = { 0.0f, 1.0f, 0.0f };
    float rgt[3]; V3Cross(upW, n, rgt);
    if (V3Norm(rgt) < 0.2f) return false;
    float upP[3]; V3Cross(n, rgt, upP); V3Norm(upP);
    float hw = g_retXrSize * 0.5f;
    for (int k = 0; k < 3; k++) {
        c[0][k] = pr[k] - rgt[k]*hw + upP[k]*hw;
        c[1][k] = pr[k] + rgt[k]*hw + upP[k]*hw;
        c[2][k] = pr[k] - rgt[k]*hw - upP[k]*hw;
        c[3][k] = pr[k] + rgt[k]*hw - upP[k]*hw;
    }
    return true;
}


static void XrRetDrawEye(int eye, const float c[4][3])
{
    if (!g_eyeFrOk || !g_bsAdd) return;
    float l = g_eyeFr[eye][0], r = g_eyeFr[eye][1];
    float t = g_eyeFr[eye][2], b = g_eyeFr[eye][3];
    float idx = 1.0f / (r - l), idy = 1.0f / (b - t);
    float ex = g_eyeOffs[eye][0], ey = g_eyeOffs[eye][1],
          ez = g_eyeOffs[eye][2];
    static const float uvs[4][2] = { {0,0}, {1,0}, {0,1}, {1,1} };
    QuadVert vq[4];
    const float* R = g_eyeRot[eye];
    for (int i = 0; i < 4; i++) {
        float hx = c[i][0] - ex, hy = c[i][1] - ey, hz = c[i][2] - ez;
        float x = hx, y = hy, z = hz;
        if (g_eyeCantCfg) {
            x = R[0]*hx + R[3]*hy + R[6]*hz;
            y = R[1]*hx + R[4]*hy + R[7]*hz;
            z = R[2]*hx + R[5]*hy + R[8]*hz;
        }
        vq[i].pos[0] = 2*idx*x + (r + l)*idx*z;
        vq[i].pos[1] = 2*idy*y + (b + t)*idy*z;
        vq[i].pos[3] = -z;
        vq[i].pos[2] = 0.5f * vq[i].pos[3];
        vq[i].uv[0] = uvs[i][0]; vq[i].uv[1] = uvs[i][1];
    }
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(g_ctx11->Map(g_retVb11, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
        return;
    memcpy(ms.pData, vq, sizeof(vq));
    g_ctx11->Unmap(g_retVb11, 0);
    UINT stride = sizeof(QuadVert), offset = 0;
    float bf[4] = { 0, 0, 0, 0 };
    g_ctx11->OMSetDepthStencilState(g_dsOff, 0);
    g_ctx11->OMSetBlendState(g_bsAdd, bf, 0xffffffff);
    g_ctx11->PSSetShaderResources(0, 1, &g_retSrv11);
    g_ctx11->IASetVertexBuffers(0, 1, &g_retVb11, &stride, &offset);
    g_ctx11->Draw(4, 0);
    g_ctx11->OMSetBlendState(NULL, NULL, 0xffffffff);
}
