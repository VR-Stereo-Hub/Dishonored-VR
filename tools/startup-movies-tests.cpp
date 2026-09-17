#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cstdint>
#include <cstdlib>
char g_dir[MAX_PATH]{};
char g_launchOrigA[1024]="Dishonored.exe -steam";
wchar_t g_launchOrigW[1024]=L"Dishonored.exe -steam";
char g_launchExtra[128]{},g_launchCmdA[2048]{};wchar_t g_launchCmdW[2048]{};
uint32_t g_launchW=0,g_launchH=0,g_resWantW=0,g_resWantH=0;
bool g_skipStartupMovies=true,g_launchResolved=false,g_launchResolving=false;
bool g_launchFull=true,g_launchVirtual=false,g_resVirtual=false,g_resWantFull=true;
int fileWrites=0;
void LaunchArgsWrite(uint32_t,uint32_t,bool){++fileWrites;}
#define DVR_LOG(...) ((void)0)
#include "startup_build.inc"
#include "startup_resolve.inc"
unsigned checks=0;
void check(bool b,const char* msg){++checks;if(!b){printf("FAIL %s\n",msg);exit(1);}}
void reset(){g_skipStartupMovies=true;g_launchResolved=false;g_launchResolving=false;LaunchArgsBuild(0,0,true,false,"host clean init");}
int main(){
 char tmp[MAX_PATH],ini[MAX_PATH];GetTempPathA(MAX_PATH,tmp);
 _snprintf(g_dir,MAX_PATH,"%sdvr-startup-%lu",tmp,GetCurrentProcessId());CreateDirectoryA(g_dir,nullptr);
 _snprintf(ini,MAX_PATH,"%s\\dishonored_vr.ini",g_dir);
 reset();LaunchArgsResolveFromIni();
 check(strstr(g_launchCmdA,"-nostartupmovies"),"clean install no mod INI suppresses startup movies");
 check(wcsstr(g_launchCmdW,L"-nostartupmovies"),"Unicode command line suppresses on clean install");
 check(!strstr(g_launchCmdA,"-ResX="),"no resolution requested on fresh install");
 check(strstr(g_launchCmdA,"-steam"),"existing launch arguments retained");
 WritePrivateProfileStringA("Other","Value","unchanged",ini);
 reset();LaunchArgsResolveFromIni();check(strstr(g_launchCmdA,"-nostartupmovies"),"INI missing Screen and Startup keys still suppresses");
 WritePrivateProfileStringA("Screen","RenderWidth","0",ini);WritePrivateProfileStringA("Screen","RenderHeight","0",ini);
 reset();LaunchArgsResolveFromIni();check(strstr(g_launchCmdA,"-nostartupmovies"),"explicit native resolution retains movie policy");
 WritePrivateProfileStringA("Screen","RenderWidth","3012",ini);WritePrivateProfileStringA("Screen","RenderHeight","3122",ini);
 reset();LaunchArgsResolveFromIni();check(strstr(g_launchCmdA,"-nostartupmovies") && strstr(g_launchCmdA,"-ResX=3012 -ResY=3122"),"resolution and movie arguments coexist");
 WritePrivateProfileStringA("Startup","SkipMovies","0",ini);
 reset();LaunchArgsResolveFromIni();check(!strstr(g_launchCmdA,"-nostartupmovies") && strstr(g_launchCmdA,"-ResX=3012"),"explicit movie opt out preserves resolution");
 char value[64];GetPrivateProfileStringA("Other","Value","",value,64,ini);check(!strcmp(value,"unchanged"),"existing config untouched by resolver");
 DeleteFileA(ini);RemoveDirectoryA(g_dir);printf("%u startup movie checks passed\n",checks);
}
