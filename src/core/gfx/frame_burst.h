// Bounded on-demand diagnostic burst. No work or storage while idle.
#pragma once
#include <cstdint>
namespace dvr::capture {
struct Burst {
    enum Phase { Idle, Armed, Capturing, Saving, Done, Failed } phase=Idle;
    uint64_t due=0,deadline=0;
    unsigned captured=0,skipped=0,wanted=16;
    uint32_t lastSerial=0;
    bool busy()const{return phase==Armed||phase==Capturing||phase==Saving;}
    bool start(uint64_t now,unsigned count=16,uint64_t delay=5000){
        if(busy()||count<1||count>32)return false;
        *this=Burst{};phase=Armed;due=now+delay;deadline=due+15000;wanted=count;return true;
    }
    bool ready(uint64_t now,uint32_t serial,bool pixels,bool capacity){
        if(phase!=Armed&&phase!=Capturing)return false;
        if(now>deadline){phase=Failed;return false;}
        if(now<due)return false;
        phase=Capturing;
        if(!pixels||!serial||serial==lastSerial)return false;
        lastSerial=serial;
        if(!capacity){++skipped;return false;}
        return true;
    }
    void queued(bool ok){
        if(!ok){phase=Failed;return;}
        if(++captured>=wanted)phase=Saving;
    }
    void saved(bool pending,bool failed){
        if(failed&&busy())phase=Failed;
        else if(phase==Saving&&!pending)phase=Done;
    }
};
} // namespace dvr::capture
