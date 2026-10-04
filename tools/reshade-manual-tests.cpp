// Native GPU test; no game, real settings or visible window.
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <imgui.h>
#include <imgui_internal.h>
#include "core/ui/reshade_panel.h"
#include "../src/core/gfx/reshade_runtime.cpp"
#undef DVR_CAT
#include "../src/core/gfx/d3d9ex.cpp"
#undef DVR_CAT
#include "../src/core/gfx/device_census.cpp"
#undef DVR_CAT
#include "../src/core/hooks/vtable.cpp"
namespace dvr::log { uint8_t g_levels[(int)Cat::COUNT]={}; void write(Cat,Level,const char* f,...){va_list a;va_start(a,f);vprintf(f,a);puts("");va_end(a);} }
namespace dvr::status { void Writer::kv(const char*,int){} void Writer::kv(const char*,unsigned long){} void Writer::kv(const char*,double){} void Writer::kv(const char*,bool){} void Writer::kv(const char*,const char*){} }
namespace dvr::native_profile { Scope::Scope(Kind k):kind(k){} void Scope::finish(){} }
namespace dvr::depthprobe { void note_texture(IDirect3DTexture9*,UINT,UINT,DWORD,D3DFORMAT){} }

static unsigned checks=0;
static void require(bool ok,const char* what) { ++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);} }
static unsigned pixel(IDirect3DDevice9* d, IDirect3DSurface9* cpu) {
    IDirect3DSurface9* bb=nullptr;require(SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb)),"backbuffer");
    require(SUCCEEDED(d->GetRenderTargetData(bb,cpu)),"readback");bb->Release();
    D3DLOCKED_RECT lock{};require(SUCCEEDED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"read pixels");
    unsigned result=*reinterpret_cast<unsigned*>(static_cast<char*>(lock.pBits)+400*lock.Pitch+400*4)&0xffffff;
    cpu->UnlockRect();return result;
}
static bool g_ovlTweakWant=false;
static ImGuiID g_ovlTweakId=0;
static constexpr float kOvlTweakMinPerSec=2, kOvlTweakMaxPerSec=30;
#include "overlay_slider_tweak.inc"
static std::map<ImGuiID,ImRect> uiBounds;
static std::map<std::string,ImRect> uiLabels;
void ImGuiTestEngineHook_ItemAdd(ImGuiContext*,ImGuiID id,const ImRect& box,const ImGuiLastItemData*) { uiBounds[id]=box; }
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags) { if(label && uiBounds.count(id))uiLabels[label]=uiBounds[id]; }
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID) { return nullptr; }
static void panel_frame() {
    uiLabels.clear();uiBounds.clear();
    ImGui::NewFrame();OvlUpdateSliderTweak();ImGui::SetNextWindowPos(ImVec2(0,0),ImGuiCond_Always);ImGui::SetNextWindowSize(ImVec2(600,900),ImGuiCond_Always);
    ImGui::Begin("F10 host");dvr::reshade_panel::draw();ImGui::End();ImGui::Render();
}
static void panel_click(const char* label,float fraction=.1f) {
    require(uiLabels.count(label)!=0,label);
    const auto box=uiLabels[label];auto& io=ImGui::GetIO();
    io.AddMousePosEvent(box.Min.x+(box.Max.x-box.Min.x)*fraction,(box.Min.y+box.Max.y)*.5f);panel_frame();
    io.AddMouseButtonEvent(0,true);panel_frame();io.AddMouseButtonEvent(0,false);panel_frame();
}
static void panel_checks(IDirect3DDevice9* dev,IDirect3DSurface9* cpu) {
    using namespace dvr::reshade_runtime;
    for(int i=0;i<90;++i) { render(dev);Sleep(10); }
    auto* ctx=ImGui::CreateContext();ctx->TestEngineHookItems=true;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(600,900);io.DeltaTime=1.0f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* atlas=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsRGBA32(&atlas,&width,&height);
    panel_frame();panel_frame();
    require(performance_mode(),"native performance setting read");
    require(api()->find_technique("Dormant.fx","Dormant").handle!=0,"disabled separate effect loads despite legacy skip setting");
    require(!api()->get_technique_state(api()->find_technique("Dormant.fx","Dormant")),"disabled effect remains unchecked");
    panel_click("Dormant");
    for(int i=0;i<150;++i) { render(dev);Sleep(10); }
    require(api()->get_technique_state(api()->find_technique("Dormant.fx","Dormant")),"previously skipped effect can be enabled in F10");
    panel_click("Dormant");
    reset();
    for(int i=0;i<150;++i) { render(dev);Sleep(10); }
    panel_frame();panel_frame();
    require(uiLabels.count("Dormant")!=0,"disabled effect control survives runtime recreation");
    require(!api()->get_technique_state(api()->find_technique("Dormant.fx","Dormant")),"unchecked selection persists across runtime recreation");
    panel_click("Enable ReShade next launch");require(!enabled_next_start() && api()!=nullptr,"startup disable persists without unloading live runtime");
    panel_click("Enable ReShade next launch");require(enabled_next_start(),"startup enable persists");
    panel_click("Effects on");require(!api()->get_effects_state(),"UI disables actual effects");
    dev->Clear(0,nullptr,D3DCLEAR_TARGET,0xff0000,1,0);render(dev);require(pixel(dev,cpu)==0xff0000,"UI effect off gives native red");
    panel_click("Effects on");require(api()->get_effects_state(),"UI enables actual effects");
    panel_click("Performance mode");require(!performance_mode(),"UI enters editable shader mode");
    for(int i=0;i<150;++i) { render(dev);Sleep(10); }
    panel_frame();panel_frame();panel_click("Test.fx");panel_frame();
    panel_click("##value",.25f);
    const auto uniform=api()->find_uniform_variable("Test.fx","Strength");require(uniform.handle!=0,"fresh uniform handle after reload");
    float strength=0;api()->get_uniform_value_float(uniform,&strength,1);
    std::printf("UI-set shader strength=%f\n",strength);require(strength>.15f && strength<.35f,"UI slider edits actual shader uniform");
    dev->Clear(0,nullptr,D3DCLEAR_TARGET,0xff0000,1,0);render(dev);const auto rgb=pixel(dev,cpu);
    require((rgb&255)>35 && (rgb&255)<90,"UI uniform changes rendered pixels");
    const float beforeNudge=strength;
    g_ovlTweakWant=true;io.AddKeyEvent(ImGuiKey_RightArrow,true);panel_frame();
    io.AddKeyEvent(ImGuiKey_RightArrow,false);panel_frame();g_ovlTweakWant=false;panel_frame();
    api()->get_uniform_value_float(api()->find_uniform_variable("Test.fx","Strength"),&strength,1);
    require(strength>beforeNudge && strength<beforeNudge+.1f,"production F10 nudge changes ReShade slider relatively");
    panel_click("Reset this setting");api()->get_uniform_value_float(api()->find_uniform_variable("Test.fx","Strength"),&strength,1);require(strength==1,"UI resets actual uniform");
    panel_click("Invert");require(!api()->get_technique_state(api()->find_technique("Test.fx","Invert")),"UI disables technique");
    panel_click("Invert");require(api()->get_technique_state(api()->find_technique("Test.fx","Invert")),"UI enables technique");
    panel_click("Performance mode");require(performance_mode(),"UI restores optimized shader mode");
    ImGui::DestroyContext(ctx);reset();
}
int main(int argc, char** argv) {
    memset(dvr::log::g_levels,2,sizeof(dvr::log::g_levels));
    const bool noEffects = argc > 1 && !strcmp(argv[1],"--disabled");
    const bool defaultOff = argc > 1 && !strcmp(argv[1],"--default-off");
    using namespace dvr::reshade_runtime;
    SetEnvironmentVariableW(L"RESHADE_DISABLE_GRAPHICS_HOOK",L"host-sentinel");
    load_optional();require(manual(),"manual selected");
    if(defaultOff) {
        require(installed(),"installed runtime discovered when disabled");require(!enabled_next_start(),"missing Enabled is off");
        require(!GetModuleHandleW(L"ReShade32.dll"),"default-off does not load ReShade DLL");require(api()==nullptr,"no public runtime while disabled");
        puts("PASS: default-off keeps installed ReShade unloaded");return 0;
    }
    wchar_t restored[64]{};GetEnvironmentVariableW(L"RESHADE_DISABLE_GRAPHICS_HOOK",restored,64);
    require(wcscmp(restored,L"host-sentinel")==0,"environment restored");
    require(createRuntime && updateRuntime && destroyRuntime,"official exports present");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DvrReShadeTest";
    require(RegisterClassW(&wc)!=0,"window class");
    HWND wnd=CreateWindowW(wc.lpszClassName,L"DVR ReShade host",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr);
    require(wnd!=nullptr,"hidden window");
    HMODULE lib=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    using Create9=HRESULT(WINAPI*)(UINT,IDirect3D9Ex**);
    auto create=reinterpret_cast<Create9>(GetProcAddress(lib,"Direct3DCreate9Ex"));
    IDirect3D9Ex* api=nullptr;require(create && SUCCEEDED(create(D3D_SDK_VERSION,&api)),"system D3D9Ex");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=wnd;pp.BackBufferWidth=640;pp.BackBufferHeight=480;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9Ex* dev=nullptr;
    require(SUCCEEDED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,wnd,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE|D3DCREATE_PUREDEVICE,&pp,nullptr,&dev)),"native device");
    dvr::d3d9ex::cs_init(); dvr::d3d9ex::g_dev=dev;
    dvr::d3d9ex::g_deviceLive=dvr::d3d9ex::g_deviceIsEx=true;
    dvr::d3d9ex::g_managed=dvr::d3d9ex::Managed::Paged;
    dvr::census::install(dev,api,0,D3DDEVTYPE_HAL,0,&pp);
    IDirect3DTexture9* before=nullptr;
    require(SUCCEEDED(dev->CreateTexture(256,256,1,0,D3DFMT_DXT5,D3DPOOL_MANAGED,&before,nullptr)),"managed before ReShade");before->Release();
    void* creationHook=(*(void***)dev)[23];
    IDirect3DSurface9* cpu=nullptr;require(SUCCEEDED(dev->CreateOffscreenPlainSurface(640,480,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr)),"readback surface");
    for(unsigned generation=0;generation<2;++generation) {
        unsigned actual=0;
        for(unsigned frame=0;frame<400;++frame) {
            require(SUCCEEDED(dev->Clear(0,nullptr,D3DCLEAR_TARGET,0x00ff0000,1,0)),"red frame");
            dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
            const D3DVIEWPORT9 vp{7,9,320,240,0,1};dev->SetViewport(&vp);
            render(dev);require((*(void***)dev)[23]==creationHook,"texture hook survives ReShade state blocks");require(runtime!=nullptr,"runtime created");require(!inside,"guard restored");
            DWORD alpha=0;dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&alpha);require(alpha==TRUE,"render state restored");
            D3DVIEWPORT9 after{};dev->GetViewport(&after);require(after.X==7 && after.Y==9 && after.Width==320 && after.Height==240,"viewport restored");
            IDirect3DTexture9* gameTex=nullptr;
            const HRESULT textureHr=dev->CreateTexture(256,256,1,0,D3DFMT_DXT5,D3DPOOL_MANAGED,&gameTex,nullptr);
            if(FAILED(textureHr)) { std::printf("vtable before=%p after=%p translations=%u calls=%u failures=%u\n",creationHook,(*(void***)dev)[23],dvr::d3d9ex::g_texTranslated,dvr::census::g_creations,dvr::census::g_failures);dvr::census::log_summary("failure"); }
            if(FAILED(textureHr)) std::printf("CreateTexture failed hr=%08lx inside=%d ex=%d\n",textureHr,int(inside),int(dvr::d3d9ex::device_is_ex()));
            require(SUCCEEDED(textureHr),"game DXT5 managed texture after ReShade");
            D3DLOCKED_RECT gameLock{};require(SUCCEEDED(gameTex->LockRect(0,&gameLock,nullptr,0)),"game texture lock after ReShade");
            memset(gameLock.pBits,0,64*gameLock.Pitch);require(SUCCEEDED(gameTex->UnlockRect(0)),"game texture upload after ReShade");gameTex->Release();
            actual=pixel(dev,cpu);if(actual==(noEffects ? 0xff0000u : 0x00ffffu) && frame>=30)break;
            MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(15);
        }
        std::printf("generation=%u pixel=%06x expected=%06x\n",generation,actual,noEffects ? 0xff0000u : 0x00ffffu);
        require(actual==(noEffects ? 0xff0000u : 0x00ffffu),"enabled/disabled preset gives expected pixels with zero Present calls");
        reset();require(runtime==nullptr,"runtime released");
        require(SUCCEEDED(dev->ResetEx(&pp,nullptr)),"reset after runtime destruction");
    }
    if(!noEffects)panel_checks(dev,cpu);
    cpu->Release();dev->Release();api->Release();DestroyWindow(wnd);
    std::printf("PASS: %u checks; effect pixels, state restore, reset/recreate, zero native Present\n",checks);
}
