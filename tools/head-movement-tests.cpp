#include <atomic>
#include <initializer_list>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define Log(...) ((void)0)
#define DVR_INFO(...) ((void)0)
#define DVR_LOG_EVERY_MS(...) ((void)0)
unsigned g_frSeen=0,g_frOurs=0,g_frStale=0,g_frReplaced=0;
float g_frWant=1;
bool g_frSaid=false,g_yawValid=true,cinematic=false,ready=true,live=true;
uint8_t pawn=0,other=0;
uint8_t* g_pePawn=&pawn;
uint8_t* g_yawPawn=&pawn;
int32_t g_yawBodyTarget=8192;
int reads=0,suspends=0;
bool RangeReadable(void* p,size_t) {return p!=nullptr;}
bool CineHeadOwnsInput(){++reads;return cinematic;}
void YawCinematicSuspend(){++suspends;}
bool YawFacingReady(){++reads;return ready;}
bool IsLiveObject(uint8_t*){++reads;return live;}
#include "../build/head-movement-tests/handler.inc"
int main(){
 int count=0;
 auto check=[&](bool ok){++count;if(!ok){printf("FAIL %d\n",count);std::exit(1);}};
 HeadMovementSet(true);
 for(int32_t yaw: {0,8192,16384,32767,-32768,-12000}){
  int32_t rot[]={300,yaw,-900},before[]={300,yaw,-900};
  FaceRotationHandler(&pawn,rot);check(!memcmp(rot,before,sizeof(rot)));
 }
 check(reads==0);check(g_frReplaced==0);
 ready=false;live=false;g_yawValid=false;
 int32_t rot[]={1,12000,2};FaceRotationHandler(&pawn,rot);check(rot[1]==12000);
 HeadMovementSet(false);ready=true;live=true;g_yawValid=true;
 FaceRotationHandler(&pawn,rot);check(rot[1]==8192 && rot[0]==1 && rot[2]==2);
 rot[1]=12000;cinematic=true;FaceRotationHandler(&pawn,rot);check(rot[1]==12000 && suspends==1);
 cinematic=false;ready=false;FaceRotationHandler(&pawn,rot);check(rot[1]==12000);
 ready=true;live=false;FaceRotationHandler(&pawn,rot);check(rot[1]==12000);
 live=true;FaceRotationHandler(&other,rot);check(rot[1]==12000);
 HeadMovementSet(true);FaceRotationHandler(nullptr,nullptr);check(g_headMovement.load());
 printf("PASS %d production head/character facing checks\n",count);
}
