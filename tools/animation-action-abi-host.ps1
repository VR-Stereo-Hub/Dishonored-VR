# Standalone ABI test; never launches the game.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo 'build/animation-action-abi'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$source=Get-Content (Join-Path $repo 'src/game/dishonored/anim_state.cpp') -Raw
$begin=$source.IndexOf('__declspec(naked) void action_stub()')
$end=$source.IndexOf('void report(', $begin)
if($begin -lt 0 -or $end -le $begin){throw 'Production stub missing'}
$stub=$source.Substring($begin,$end-$begin)
$prefix=@'
#include <cstdint>
#include <cstdio>
bool deny=false;
unsigned seenThis=0,seenRequest=0;
bool __cdecl reject_action(uint8_t* machine,uint8_t* request) {
    seenThis=(unsigned)machine;seenRequest=(unsigned)request;return deny;
}
__declspec(naked) void native_tail() {
    __asm {
        cmp dword ptr [ebp-4],-1
        jne bad
        mov eax,[ebp+8]
        add eax,[ebp+12]
        add eax,[ebp+16]
        add eax,ecx
        mov esp,ebp
        pop ebp
        ret 12
    bad:
        int 3
    }
}
uintptr_t actionResume=(uintptr_t)native_tail;
'@
$suffix=@'
int main(){
    using Fn=int(__thiscall*)(void*,void*,void*,int);
    Fn fn=(Fn)action_stub;
    int failed=0;unsigned before=0,after=0;
    for(int n=0;n<1000;++n){
        deny=(n&1)!=0;
        __asm mov before,esp
        int answer=fn((void*)11,(void*)17,(void*)23,29);
        __asm mov after,esp
        if(answer!=(deny?0:80) || before!=after || seenThis!=11 || seenRequest!=17)++failed;
    }
    std::printf("1000 production action-stub ABI calls, %d failures\n",failed);
    return failed?1:0;
}
'@
[IO.File]::WriteAllText((Join-Path $out 'action-stub.cpp'),$prefix+"`r`n"+$stub+"`r`n"+$suffix)
. (Join-Path $PSScriptRoot 'lib/msvc.ps1')
$root=Get-DvrMsvcRoot
$sdk=(Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$lib=(Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib' | Sort-Object Name -Descending | Select-Object -First 1).FullName
$oldInclude=$env:INCLUDE;$oldLib=$env:LIB
$env:INCLUDE="$root\include;$sdk\ucrt;$sdk\shared;$sdk\um"
$env:LIB="$root\lib\x86;$lib\ucrt\x86;$lib\um\x86"
Push-Location $out
try {
    & "$root\bin\Hostx64\x86\cl.exe" /nologo /EHsc /W4 /std:c++17 action-stub.cpp /Fe:action-stub.exe
    if($LASTEXITCODE -ne 0){throw 'ABI compilation failed'}
    .\action-stub.exe
    if($LASTEXITCODE -ne 0){throw 'ABI validation failed'}
} finally {Pop-Location;$env:INCLUDE=$oldInclude;$env:LIB=$oldLib}
