// VR-43: suppress only the native GFx stripe-visibility query. No HUD fields
// are stored or retained. The native movie still owns animation and its cache.
namespace {
std::atomic<bool> g_letterboxHide{false};
bool g_letterboxHook=false,g_letterboxFailed=false;
uint32_t g_letterboxQueries=0,g_letterboxHidden=0;
double g_letterboxLogAfter=0;
using LetterboxQueryFn=int (__thiscall*)(void*,uint32_t);
int LetterboxResult(int native,bool enabled,bool projection,bool session,bool menu,bool matching) {
    return enabled && projection && session && !menu && matching ? 0 : native;
}
__declspec(noinline) int __fastcall LetterboxQueryStub(void* hud,void*,uint32_t mask) {
    // Preserve the original call and return when disabled. The native caller
    // supplies the current HUD; this wrapper never dereferences or writes it.
    const int native=((LetterboxQueryFn)kLetterboxAnyMask)(hud,mask);
    const bool matching=(uintptr_t)_ReturnAddress()==kLetterboxQueryReturn && mask==kLetterboxMask;
    const bool enabled=g_letterboxHide.load(),projection=dvr::stereo::wants_projection();
    const bool session=dvr::vr::session_live(),menu=g_menuOpen || g_inMenu || g_mainMenu;
    const int result=LetterboxResult(native,enabled,projection,session,menu,matching);
    ++g_letterboxQueries;
    if(native && !result) ++g_letterboxHidden;
    const double now=MaimNowMs();
    if(now>=g_letterboxLogAfter) {
        g_letterboxLogAfter=now+3000;
        Log("cine/borders: query=%u hidden=%u native=%d returned=%d enabled=%d match=%d projection=%d session=%d menu=%d HUD=%p; SetBlackStripes query only",
            g_letterboxQueries,g_letterboxHidden,native,result,(int)enabled,(int)matching,(int)projection,(int)session,(int)menu,hud);
    }
    return result;
}
bool LetterboxFingerprint(const uint8_t* bytes) {
    if(memcmp(bytes,kLetterboxQueryOrig,sizeof(kLetterboxQueryOrig))) return false;
    int32_t displacement=0; memcpy(&displacement,bytes+3,sizeof(displacement));
    return kLetterboxQuerySetup+sizeof(kLetterboxQueryOrig)+displacement==kLetterboxAnyMask;
}
}
static bool CineBordersEnabled() { return g_letterboxHide.load(); }
static void CineBordersSet(bool on) {
    g_letterboxHide.store(on);
    Log("cine/borders: %s (live); changes stripe visibility on the next HUD movie update",on ? "hide" : "native");
}
static void CineBordersConfigure(const char* ini) {
    g_letterboxHide.store(GetPrivateProfileIntA("Cine","HideBorders",1,ini)!=0);
    Log("cine/borders: %s ([Cine] HideBorders); default off, no viewport or FOV changes",g_letterboxHide.load() ? "hide" : "native");
}
// Patch once on the game/script lane, as with the existing scene draw hook.
// Live OFF forwards through the original query; no repeated code patching.
static void CineBordersApply() {
    if(!g_letterboxHide.load() || g_letterboxHook || g_letterboxFailed) return;
    auto* site=(uint8_t*)kLetterboxQuerySetup;
    if(!RangeReadable(site,sizeof(kLetterboxQueryOrig)) || !LetterboxFingerprint(site)) {
        g_letterboxFailed=true;
        Log("cine/borders: REFUSED hook at %08x; seven-byte setup or decoded target mismatch; native stripes retained",(unsigned)kLetterboxQuerySetup);
        return;
    }
    DWORD oldProtection=0;
    if(!VirtualProtect(site,sizeof(kLetterboxQueryOrig),PAGE_EXECUTE_READWRITE,&oldProtection)) {
        g_letterboxFailed=true; Log("cine/borders: REFUSED VirtualProtect error=%lu",GetLastError()); return;
    }
    const int32_t relative=(int32_t)((uintptr_t)&LetterboxQueryStub-kLetterboxQueryReturn);
    memcpy(site+3,&relative,sizeof(relative));
    DWORD ignored=0;
    const bool protectedAgain=VirtualProtect(site,sizeof(kLetterboxQueryOrig),oldProtection,&ignored)!=0;
    FlushInstructionCache(GetCurrentProcess(),site,sizeof(kLetterboxQueryOrig));
    g_letterboxHook=true;
    Log("cine/borders: installed verified stripe-only query hook at %08x -> original %08x (thiscall, mask argument, ret4); protectionRestored=%d",
        (unsigned)(kLetterboxQuerySetup+2),(unsigned)kLetterboxAnyMask,(int)protectedAgain);
}
