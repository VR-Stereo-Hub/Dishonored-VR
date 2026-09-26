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
    p.sceneDepth=depth.srv;run(d,gpu,old,out,p);p.historyValid=true;
    check("audit: changed textured surface executes",run(d,gpu,cur,out,p));
    Img changed=read(d,out);double error=0;
    for(UINT y=4;y<H-4;++y)for(UINT x=4;x<W-4;++x)error+=fabs(changed.at(x,y)-cur.at(x,y));
    error/=(W-8)*(H-8);
    printf("AUDIT changed textured surface: mean gamma error %.6f; dark pixel %.6f (current 0.2), light %.6f (current 0.8)\n",error,changed.at(80,80),changed.at(81,80));
    check("regression: changed stationary-camera pattern rejects stale colour",error<2.01f/255);
    // Explicit invalidation is the control: exactly the same image without old history.
    p.historyValid=false;run(d,gpu,cur,out,p);Img clean=read(d,out);float worst=0;
    for(size_t k=0;k<cur.g.size();++k)worst=fmaxf(worst,fabsf(clean.g[k]-cur.g[k]));
    check("audit: invalidation removes the trail",worst<2.01f/255);
    p.eye=1;run(d,gpu,cur,out,p);uint64_t before=gpu.bytes();
    p.sceneDepth=nullptr;p.historyValid=true;run(d,gpu,cur,out,p);uint64_t after=gpu.bytes();
    printf("AUDIT single depth miss: bytes %llu -> %llu (fused path has no vector allocations)\n",(unsigned long long)before,(unsigned long long)after);
    check("regression: fallback has no allocation churn",before==after&&!gpu.vectors(0)&&!gpu.vectors(1));
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
    check("regression: stationary isolated detail retains brightness",seeded>0.99f&&attenuated>0.99f);

    // Visibility test has camera motion, so the quiet-camera colour response cannot hide it.
    Img surface=make(W,H),exposed=make(W,H);
    for(UINT y=0;y<H;++y)for(UINT x=0;x<W;++x){
        surface.at(x,y)=0.54f+0.2f*sinf(x*0.18f);
        exposed.at(x,y)=0.50f+0.2f*sinf((x+2)*0.18f);
    }
    Src nearDepth=upload_depth(d,W,H,std::vector<float>(W*H,1));
    PassParams vis;vis.temporal=true;vis.depthScale=100;float errs[2]={};
    for(int reject=0;reject<2;++reject){
        vis.historyValid=false;vis.sceneDepth=reject?nearDepth.srv:depth.srv;
        run(d,gpu,surface,out,vis);
        vis.historyValid=true;vis.sceneDepth=depth.srv;vis.translation[1]=5;
        run(d,gpu,exposed,out,vis);Img result=read(d,out);
        for(UINT y=8;y<H-8;++y)for(UINT x=8;x<W-8;++x)errs[reject]+=fabsf(result.at(x,y)-exposed.at(x,y));
        errs[reject]/=(W-16)*(H-16);
    }
    printf("REGRESSION visibility: matching-depth control %.6f, disoccluded %.6f\n",errs[0],errs[1]);
    check("regression: depth visibility rejects colour that clipping would keep",errs[1]<0.005f&&errs[0]>0.015f);
    nearDepth.release();
    vis.materializeVectors=true;vis.translation[1]=0;vis.prevTanH=1;vis.prevTanV=1;vis.tanH=0.8f;vis.tanV=0.8f;
    run(d,gpu,exposed,out,vis);auto fov=read_vectors(d,gpu.vectors(0));
    const UINT fx=120,fy=80;const float u=(fx+0.5f)/W;
    check("regression: reprojection uses the previous projection",fov.size()==W*H*4&&fabsf(fov[(fy*W+fx)*4]-((u-0.5f)*0.8f+0.5f-u))<0.0001f);
    View a,b;a.ok=b.ok=true;a.tanH=b.tanH=a.tanV=b.tanV=1;a.w=b.w=W;a.h=b.h=H;a.timeMs=100;b.timeMs=111;
    check("regression: consecutive views keep history",keep_history(a,b)==Reset::None);
    b.timeMs=250;check("regression: long capture gap rejects history",keep_history(a,b)==Reset::Record);
    b.timeMs=111;b.cameraIdentity=5;check("regression: camera identity change rejects history",keep_history(a,b)==Reset::Record);
    b.cameraIdentity=0;b.sceneEpoch=1;check("regression: same-camera level/menu transition rejects history",keep_history(a,b)==Reset::Record);
    b.sceneEpoch=0;b.yaw=NAN;check("regression: nonfinite camera rejects history",keep_history(a,b)==Reset::Record);
    check("regression: response has the same decay at 45 and 90 Hz",
        fabsf(1-blend_for_interval(0.2f,1000.0/45)-powf(1-blend_for_interval(0.2f,1000.0/90),2))<0.00001f &&
        blend_for_interval(0.6f,1000.0/45)>0.6f);
    // Saturated colours, not just grey: a stationary red/green/blue point retains all channels.
    std::vector<uint32_t> rgb(W*H,0xff000000);rgb[(H/2)*W+W/2]=0xff0000ff;
    rgb[(H/2)*W+W/2+5]=0xff00ff00;rgb[(H/2)*W+W/2+10]=0xffff0000;
    D3D11_TEXTURE2D_DESC cd={};cd.Width=W;cd.Height=H;cd.MipLevels=cd.ArraySize=1;
    cd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;cd.SampleDesc.Count=1;cd.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA cs={rgb.data(),W*4,0};Src colors;
    bool colorOk=SUCCEEDED(d.dev->CreateTexture2D(&cd,&cs,&colors.tex))&&SUCCEEDED(d.dev->CreateShaderResourceView(colors.tex,nullptr,&colors.srv));
    PassParams cp;cp.temporal=true;cp.w=cp.ow=W;cp.h=cp.oh=H;
    for(int frame=0;frame<40&&colorOk;++frame){colorOk=gpu.run(d.dev,d.ctx,colors.srv,out.rtv,cp,why,sizeof(why));cp.historyValid=true;}
    auto rgbHistory=read_vectors(d,gpu.history(0));colorOk=colorOk&&rgbHistory.size()==W*H*4;
    for(int channel=0;channel<3&&colorOk;++channel){
        size_t k=((H/2)*W+W/2+channel*5)*4;
        for(int j=0;j<3;++j)colorOk=colorOk&&fabsf(rgbHistory[k+j]-(j==channel?1.0f:0.0f))<0.001f;
    }
    check("regression: saturated stationary details retain hue and energy",colorOk);colors.release();
    gpu.shutdown();check("regression: explicit lifecycle reset can reinitialize",gpu.init(d.dev,why,sizeof(why))&&gpu.bytes()==0);
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
    printf("taa-audit: %d total checks, %d failures; regression and characterization suite\n",g_checks,g_failed);
    return g_failed?1:0;
}
