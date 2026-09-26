// Characterize production TAA limitations. No game or installed configuration changes.
#define main clarity_baseline_main
#include "clarity-gpu-tests.cpp"
#undef main
#include <algorithm>

static double time_pass(Dev& d, Gpu& gpu, Src& src, Dst& out, PassParams p) {
    ID3D11Query *dis=nullptr,*start=nullptr,*end=nullptr;
    D3D11_QUERY_DESC q={D3D11_QUERY_TIMESTAMP_DISJOINT,0};
    if(FAILED(d.dev->CreateQuery(&q,&dis))) return -1;
    q.Query=D3D11_QUERY_TIMESTAMP;
    d.dev->CreateQuery(&q,&start);d.dev->CreateQuery(&q,&end);
    if(!start||!end) {if(start)start->Release();if(end)end->Release();dis->Release();return -1;}
    char why[256]={}; std::vector<double> times;
    p.w=out.w;p.h=out.h;p.ow=out.w;p.oh=out.h;
    bool ok=true;
    for(int j=0;j<36&&ok;++j) {
        d.ctx->Begin(dis); d.ctx->End(start);
        ok=gpu.run(d.dev,d.ctx,src.srv,out.rtv,p,why,sizeof(why));
        d.ctx->End(end);d.ctx->End(dis);d.ctx->Flush();
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT info={}; UINT64 a=0,b=0;
        DWORD until=GetTickCount()+5000;HRESULT hr;
        while((hr=d.ctx->GetData(dis,&info,sizeof(info),0))==S_FALSE && (LONG)(GetTickCount()-until)<0) Sleep(1);
        ok=ok&&hr==S_OK&&!info.Disjoint&&info.Frequency&&
           d.ctx->GetData(start,&a,sizeof(a),0)==S_OK&&d.ctx->GetData(end,&b,sizeof(b),0)==S_OK;
        if(ok&&j>=12) times.push_back(1000.0*(b-a)/info.Frequency);
    }
    dis->Release();start->Release();end->Release();
    if(!ok||times.empty())return -1;
    std::sort(times.begin(),times.end());return times[times.size()/2];
}
int main() {
    const int baseline=clarity_baseline_main();if(baseline)return baseline;
    Dev d;if(!make_device(d))return 2;Gpu gpu;char why[256]={};
    if(!gpu.init(d.dev,why,sizeof(why)))return 3;
    const UINT W=160,H=160;Dst out=make_dst(d,W,H);PassParams p;p.temporal=true;
    Src depth=upload_depth(d,W,H,std::vector<float>(W*H,2));
    Img old=make(W,H),cur=make(W,H);
    for(UINT y=0;y<H;++y)for(UINT x=0;x<W;++x){old.at(x,y)=((x+y)&1)?0.2f:0.8f;cur.at(x,y)=1-old.at(x,y);}
    run(d,gpu,old,out,p);p.historyValid=true;p.sceneDepth=depth.srv;
    check("audit: changed textured surface executes",run(d,gpu,cur,out,p));
    Img changed=read(d,out);double error=0;
    for(UINT y=4;y<H-4;++y)for(UINT x=4;x<W-4;++x)error+=fabs(changed.at(x,y)-cur.at(x,y));
    error/=(W-8)*(H-8);
    printf("AUDIT changed textured surface: mean gamma error %.6f; dark pixel %.6f (current 0.2), light %.6f (current 0.8)\n",error,changed.at(80,80),changed.at(81,80));
    check("audit: moving-pattern trail is reproduced, not prevented by clipping",error>0.2);
    // Explicit invalidation is the control: exactly the same image without old history.
    p.historyValid=false;run(d,gpu,cur,out,p);Img clean=read(d,out);float worst=0;
    for(size_t k=0;k<cur.g.size();++k)worst=fmaxf(worst,fabsf(clean.g[k]-cur.g[k]));
    check("audit: invalidation removes the trail",worst<2.01f/255);
    p.eye=1;run(d,gpu,cur,out,p);uint64_t before=gpu.bytes();
    p.sceneDepth=nullptr;p.historyValid=true;run(d,gpu,cur,out,p);uint64_t after=gpu.bytes();
    printf("AUDIT single depth miss: bytes %llu -> %llu (two vector textures discarded)\n",(unsigned long long)before,(unsigned long long)after);
    check("audit: one fallback discards both eye vector allocations",before-after==uint64_t(W)*H*8*2);
    p.eye=0;p.historyValid=false;EdgeScene sc;Img still=render_edge(sc,basis_from_rotator(0,0,0),W,H,1);
    Img truth=render_edge(sc,basis_from_rotator(0,0,0),W,H,16);
    for(int i=0;i<40;++i){run(d,gpu,still,out,p);p.historyValid=true;}
    Img settled=read(d,out);float rawErr=edge_error(still,truth),taaErr=edge_error(settled,truth);
    printf("AUDIT static slanted edge: raw %.6f, TAA after 40 identical frames %.6f\n",rawErr,taaErr);
    check("audit: no static supersampling without new subpixel samples",fabsf(rawErr-taaErr)<0.003f);
    p.sharpen=0.4f; // Default sharpening must not hide the stationary-detail test.
    Img point=make(W,H,0);point.at(W/2,H/2)=1;
    p.historyValid=false;run(d,gpu,point,out,p);float seeded=read(d,out).at(W/2,H/2);
    p.historyValid=true;
    for(int i=0;i<40;++i)run(d,gpu,point,out,p);
    float attenuated=read(d,out).at(W/2,H/2);
    printf("AUDIT stationary white point: seeded %.6f -> accumulated %.6f gamma (%.6f linear)\n",seeded,attenuated,to_linear(attenuated));
    check("audit: variance clipping attenuates a stationary isolated detail",seeded>0.99f&&attenuated<0.85f);
    depth.release();out.release();gpu.trim(false,false);
    // Production-size GPU timestamp measurements exclude uploads, readback and D3D9 interop.
    Dst large=make_dst(d,2750,2850);Src source=upload(d,make(2750,2850,0.4f));
    // Match the production shared-depth format, including unused RGB channels.
    Src largeDepth;D3D11_TEXTURE2D_DESC td={};td.Width=2750;td.Height=2850;
    td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    std::vector<uint16_t> dp(size_t(td.Width)*td.Height*4,0);
    for(size_t i=3;i<dp.size();i+=4)dp[i]=0x4000;
    D3D11_SUBRESOURCE_DATA sd={dp.data(),td.Width*8,0};
    check("audit: production-format benchmark depth allocated",SUCCEEDED(d.dev->CreateTexture2D(&td,&sd,&largeDepth.tex))&&
          SUCCEEDED(d.dev->CreateShaderResourceView(largeDepth.tex,nullptr,&largeDepth.srv)));
    if(!largeDepth.srv)return 4;
    dp.clear();dp.shrink_to_fit();
    PassParams bench;bench.sharpen=0.4f;
    double base=time_pass(d,gpu,source,large,bench);
    bench.temporal=true;bench.historyValid=true;
    double rotation=time_pass(d,gpu,source,large,bench);
    bench.sceneDepth=largeDepth.srv;
    double motion=time_pass(d,gpu,source,large,bench);
    printf("AUDIT GPU %s 2750x2850, median of 24 after 12 warmups, one eye: sharpen %.4f ms, rotation TAA+sharpen %.4f ms, vector TAA+sharpen %.4f ms\n",d.kind,base,rotation,motion);
    check("audit: GPU timestamps are valid",base>0&&rotation>0&&motion>0);
    largeDepth.release();source.release();large.release();gpu.shutdown();d.ctx->Release();d.dev->Release();
    printf("taa-audit: %d total checks, %d failures; reproduced limitations are not fixed\n",g_checks,g_failed);
    return g_failed?1:0;
}
