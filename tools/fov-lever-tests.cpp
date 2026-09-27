#include "../src/game/dishonored/fov_lever_policy.h"
#include "../src/game/dishonored/cinematic_fov_policy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
unsigned checks=0;
void check(bool v,const char* msg) { ++checks; if(!v) { std::fprintf(stderr,"FAIL: %s\n",msg); std::exit(1); } }
bool near(float a,float b) { return std::fabs(a-b)<0.002f; }
float old_target(float sensor,float natural,float target) {
    float t=(sensor>natural?natural:sensor)*(target/natural);
    return t<20?20:(t>160?160:t);
}
int main() {
    using dvr::fov_lever::target;
    // Negative control: delayed engine readback reproduces the released bug.
    float old=110;
    for(int i=0;i<20000;++i) old += 0.1f*(old_target(old,110,108.0666f)-old);
    check(near(old,20),"released recurrence collapses to 20 degrees");
    for(float requested:{40.0f,90.0f,103.0f,108.0666f,110.0f,111.0f,137.0f,160.0f}) {
        for(float blend:{0.01f,0.1f,0.5f,1.0f}) {
            float sensor=110;
            for(int i=0;i<20000;++i) {
                const float written=target(sensor,110,requested);
                check(written>=requested-0.001f,"normal readback never contracts below requested FOV");
                // Multiple dispatches before the engine updates its sensor.
                check(written==target(sensor,110,requested),"delayed readback does not compound dispatches");
                sensor += blend*(written-sensor);
            }
            check(near(sensor,requested),"interpolated readback converges to requested FOV");
        }
    }
    check(near(target(32,110,108.0666f),32),"native narrow zoom passes through without contraction");
    check(near(target(32,75,108),old_target(32,75,108)),"existing expansion zoom scaling is unchanged");
    // A game-authored gradual zoom below the ceiling must not be flattened.
    for(float sensor=108; sensor>=32; sensor-=0.25f)
        check(near(target(sensor,110,108.0666f),sensor),"smooth native zoom retained");
    // Native unzoom supplies new readback, which must reach the new ceiling.
    for(float sensor=32; sensor<=110; sensor+=0.25f)
        check(near(target(sensor,110,108.0666f),sensor<108.0666f?sensor:108.0666f),"native zoom recovery retained");
    check(near(target(111,110,108),108),"target decrease caps once");
    check(near(target(108,110,111),108*(111.0f/110)),"target increase keeps old widening behavior");
    check(near(target(108,108,108),108),"recaptured own output is stable");
    check(near(target(110,110,108.0666f),108.0666f),"fresh level baseline is stable");
    for(float invalid:{0.0f,175.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        check(target(invalid,110,108)==0,"invalid sensor refuses");
    check(target(110,0,108)==0 && target(110,175,108)==0,"invalid baseline refuses");
    check(target(110,110,0)==0 && target(110,110,175)==0,"invalid target refuses");
    // Draw-scoped FOV continues to restore the stable persistent FOV each draw.
    float sensor=110;
    for(int i=0;i<2000;++i) {
        sensor += 0.1f*(target(sensor,110,108.0666f)-sensor);
        float cache=sensor;
        dvr::cine_fov::Scope scope;
        check(scope.begin(&cache,dvr::cine_fov::gameplay_target(cache,108.0666f,103),true),"gameplay scope enters");
        check(scope.end(true) && cache==sensor,"gameplay scope restores persistent source");
    }
    check(near(sensor,108.0666f),"gameplay draw scopes cannot restart narrowing");
    check(near(dvr::cine_fov::gameplay_target(sensor,108.0666f,103),103),"unchanged projection setting converges to 103");
    // VR-227: replay the reported narrow dialogue source through the actual
    // persistent and draw-exit policies. The old writer freezes at 51.60,
    // then the three-second bridge exposes a 47.60-degree gameplay claim.
    {
        float stuck=51.60f;
        dvr::cine_fov::ExitBridge exit;
        exit.update(true,false,stuck,108.0666f,1000);
        for(unsigned ms=1010;ms<=5000;ms+=10) {
            stuck += 0.1f*(target(stuck,110,108.0666f)-stuck);
            exit.update(false,true,stuck,108.0666f,ms);
        }
        check(near(stuck,51.60f) && !exit.primed,"1.0.1 retains narrow source after exit bridge expires");
        check(std::fabs(dvr::cine_fov::gameplay_target(stuck,108.0666f,103)-47.60f)<0.02f,
            "negative control reproduces reported narrow gameplay claim");
    }
    for(float natural:{75.0f,110.0f}) for(float blend:{0.05f,0.1f,0.5f,1.0f}) {
        dvr::fov_lever::CinematicRecovery recovery;
        float readback=51.60f;
        check(recovery.update(true,true,false,readback,108.0666f,1000)>0,"dialogue owns persistent target");
        for(unsigned ms=1010;ms<=7000;ms+=10) {
            const float forced=recovery.update(true,false,true,readback,108.0666f,ms);
            const float written=forced>0?forced:target(readback,natural,108.0666f);
            readback += blend*(written-readback);
        }
        check(std::fabs(readback-108.0666f)<0.51f,"exit repairs persistent source at both natural FOV baselines");
        check(!recovery.bridge.primed,"converged recovery releases");
        check(recovery.update(true,false,true,32,108.0666f,7100)==0,"later gameplay zoom cannot restart recovery");
    }
    {
        dvr::fov_lever::CinematicRecovery recovery;
        recovery.update(true,true,false,52,108,1000);
        check(recovery.update(false,false,true,52,108,1100)==0,"disabled, menu or invalid state cancels recovery");
        check(recovery.update(true,false,true,52,108,1200)==0,"reentry cannot retain canceled cinematic intent");
        recovery.update(true,true,false,52,108,2000);
        recovery = {}; // Production resets on UI epoch, failed ownership or new baseline.
        check(recovery.update(true,false,true,52,108,2100)==0,"owner lifecycle reset cannot carry recovery to replacement");
        recovery.update(true,true,false,52,108,3000);
        check(recovery.update(true,false,false,52,108,3100)==0,"non-locomotion state cancels exit suppression");
        recovery.update(true,true,false,52,108,4000);
        check(recovery.update(true,false,true,52,108,4100)==108,"narrow exit starts bounded repair");
        check(recovery.update(true,false,true,52,108,7100)==0,"unresponsive camera releases at three seconds");
        recovery.update(true,true,false,52,108,8000);
        recovery.update(true,false,true,52,108,8100);
        check(recovery.update(true,false,true,52,108,8000)==0,"clock rollback cancels repair");
        check(recovery.update(true,true,false,52,111,9000)==111,"cinematic resolution change follows current target");
    }
    // ---- the re-armed base (a load, a death, a store exit) ------------------------
    //
    // Model of the game side: the lever writes its value into the controller's
    // DefaultFOV as well as the live fields, a transient (store/death camera) forces
    // the rendered FOV narrow for a while, and when it ends the game restores the
    // live FOV FROM DefaultFOV and interpolates. Returns the settled rendered FOV.
    auto afterTransient = [](float natural, float requested, float narrow) {
        float sensor = requested, def = requested;
        for (int i = 0; i < 300; ++i) {          // the transient owns the view
            sensor = narrow;
            def = target(sensor, natural, requested);
        }
        sensor = def;                            // the game restores its default
        for (int i = 0; i < 5000; ++i) {
            const float t = target(sensor, natural, requested);
            def = t;
            sensor += 0.1f * (t - sensor);
        }
        return sensor;
    };
    {
        using dvr::fov_lever::rearm_natural;
        using dvr::fov_lever::Rearm;
        const float req = 108.0666f;
        // Negative control: the recapture the logs show (our own 108.07 read back as
        // the natural base) leaves the lever at ratio 1 and a 37.36 transient sticks.
        check(std::fabs(afterTransient(req, req, 37.36f) - 37.36f) < 0.01f,
              "negative control: an echoed base keeps the store/death narrowing (the reported small box)");
        check(std::fabs(afterTransient(75.0f, req, 37.36f) - req) < 0.01f,
              "the real base widens a store/death narrowing back to the target");
        check(std::fabs(afterTransient(75.0f, req, 60.0f) - req) < 0.01f, "a 60 degree transient also recovers");
        Rearm why;
        check(near(rearm_natural(75.0f, 0, 0, req, &why), 75.0f) && why == Rearm::Fresh, "first capture is taken as read");
        check(near(rearm_natural(108.07f, 75.0f, req, req, &why), 75.0f) && why == Rearm::KeptEcho,
              "our own output read back after a load is not a base");
        check(near(rearm_natural(108.3f, 75.0f, 90.0f, req, &why), 75.0f) && why == Rearm::KeptAtTarget,
              "a reading at the target cannot be told from our output");
        check(near(rearm_natural(37.4f, 75.0f, 53.8f, req, &why), 75.0f) && why == Rearm::KeptNarrower,
              "a narrowing captured on a load (37.4) is not a base");
        check(near(rearm_natural(60.0f, 75.0f, 86.5f, req, &why), 75.0f) && why == Rearm::KeptNarrower,
              "a narrowing captured on a load (60) is not a base");
        check(near(rearm_natural(85.0f, 75.0f, req, req, &why), 85.0f) && why == Rearm::Accepted,
              "a wider game FOV option is accepted");
        check(near(rearm_natural(75.2f, 75.0f, req, req, &why), 75.2f) && why == Rearm::Accepted,
              "the same base read again is accepted");
        check(rearm_natural(20.0f, 75.0f, req, req, &why) == 0 && why == Rearm::Invalid, "out of range refuses");
        check(rearm_natural(std::numeric_limits<float>::quiet_NaN(), 75.0f, req, req, &why) == 0, "NaN refuses");
        // Landscape target (a 110.9 claim): our 110.9 echo is kept out the same way.
        check(near(rearm_natural(110.9f, 75.0f, 110.9f, 110.9f, &why), 75.0f) && why == Rearm::KeptEcho,
              "landscape echo is kept out too");
        // End to end: every load re-reads the sensor; the base must stay 75 through
        // any number of loads, transients included, and the view must come back.
        float kept = 0, last = 0, view = 75.0f;
        for (int load = 0; load < 6; ++load) {
            const float nat = rearm_natural(view, kept, last, req, &why);
            check(near(nat, 75.0f), "the base survives every load");
            kept = nat;
            for (int i = 0; i < 2000; ++i) { last = target(view, kept, req); view += 0.1f * (last - view); }
            check(near(view, req), "the view settles at the target after the load");
            view = (load % 2) ? 37.36f : 60.0f;    // the next load happens during a narrowing
        }
    }
    std::printf("FOV feedback: %u checks passed; old writer collapses, fixed writer remains stable.\n",checks);
}
