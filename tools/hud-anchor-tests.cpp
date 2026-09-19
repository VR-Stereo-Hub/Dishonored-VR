// tools/hud-anchor-tests.cpp - the HUD anchors' placement math on the host
// (VR-117). Run by tools/hud-anchor-host.ps1; never launches the game.
#include "core/vr/hud_anchor.h"
#include "core/gfx/hud_marker.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
static unsigned checks = 0;
static void check(bool value, const char* why) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
static bool near3(const float a[3], float x, float y, float z, float eps = 1e-4f) {
    return std::fabs(a[0] - x) < eps && std::fabs(a[1] - y) < eps && std::fabs(a[2] - z) < eps;
}
static void rot(const float q[4], const float v[3], float out[3]) {
    dvr::xrmath::quat_rotate(q[0], q[1], q[2], q[3], v, out);
}
int main() {
    using namespace dvr::hudanchor;
    const float zAxis[3] = {0, 0, 1}, yAxis[3] = {0, 1, 0}, xAxis[3] = {1, 0, 0};
    float q[4], o[3];
    const float grip[3]={.2f,1.1f,-.5f},eyeQ[4]={0,0,0,1};
    camera_panel_position(grip,eyeQ,-.05f,o);
    check(near3(o,.2f,1.1f,-.45f),"reading panel moves5cm toward camera from grip");
    const float movedGrip[3]={.3f,1.3f,-.6f};
    camera_panel_position(movedGrip,eyeQ,-.05f,o);
    check(near3(o,.3f,1.3f,-.55f),"reading panel follows hand translation exactly");
    camera_panel_position(grip,eyeQ,0,o);
    check(near3(o,grip[0],grip[1],grip[2]),"zero distance centers reading panel on hand");
    // The watch-face tilt maps the panel's normal (+Z) onto the back of the
    // hand (grip -X right, +X left) and keeps the panel's up along grip +Y.
    tilt_right(q); rot(q, zAxis, o); check(near3(o, -1, 0, 0), "right tilt: panel normal = grip -X (the back of the right hand)");
    rot(q, yAxis, o);               check(near3(o, 0, 1, 0), "right tilt: panel up = grip +Y");
    rot(q, xAxis, o);               check(near3(o, 0, 0, 1), "right tilt: panel right = grip +Z");
    tilt_left(q);  rot(q, zAxis, o); check(near3(o, 1, 0, 0), "left tilt: panel normal = grip +X (the back of the left hand)");
    rot(q, yAxis, o);               check(near3(o, 0, 1, 0), "left tilt: panel up = grip +Y");
    // Unit quaternions.
    for (int h = 0; h < 2; ++h) {
        if (h) tilt_right(q); else tilt_left(q);
        check(std::fabs(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3] - 1.0f) < 1e-5f, "tilt is a unit quaternion");
    }
    // FollowGrip with an identity grip and no user tilt equals the tilt itself;
    // a +90 nod turns the panel's normal from -X toward... the nod is about the
    // panel's own right axis (grip +Z for the right hand), so the normal moves in
    // the grip X/Y plane.
    {
        const float ident[4] = {0, 0, 0, 1};
        float fq[4];
        follow_grip_orientation(ident, 1, 0.0f, fq);
        rot(fq, zAxis, o); check(near3(o, -1, 0, 0), "follow grip, identity grip, right: normal = -X");
        follow_grip_orientation(ident, 1, 90.0f, fq);
        rot(fq, zAxis, o); check(std::fabs(o[2]) < 1e-4f, "a nod about the panel's right axis keeps the normal in the grip X/Y plane");
        check(std::fabs(std::fabs(o[1]) - 1.0f) < 1e-4f, "a 90 degree nod turns the normal onto grip Y");
        // A yawed grip carries the panel with it.
        float yaw90[4]; dvr::xrmath::quat_axis_angle(0, 1, 0, 3.14159265f * 0.5f, yaw90);
        follow_grip_orientation(yaw90, 1, 0.0f, fq);
        rot(fq, zAxis, o); check(near3(o, 0, 0, 1), "a +90 yaw grip turns the right hand's normal from -X to +Z");
        // VR-142: the spin turns the panel in its own plane and never moves its normal.
        float sq[4];
        follow_grip_orientation(ident, 1, 0.0f, sq, 90.0f);
        rot(sq, zAxis, o); check(near3(o, -1, 0, 0), "a spin keeps the panel's normal on the back of the hand");
        rot(sq, xAxis, o); check(near3(o, 0, 1, 0), "a +90 spin turns the panel's right (grip +Z) onto its up (grip +Y)");
        follow_grip_orientation(ident, 1, 0.0f, sq, 0.0f);
        follow_grip_orientation(ident, 1, 0.0f, fq);
        check(std::fabs(sq[0]-fq[0]) + std::fabs(sq[1]-fq[1]) + std::fabs(sq[2]-fq[2]) + std::fabs(sq[3]-fq[3]) < 1e-6f,
              "spin 0 is the old orientation exactly");
    }
    // Wrist position: grip + R(q)*offset + lift along world up.
    {
        const float grip[3] = {1, 2, 3};
        const float ident[4] = {0, 0, 0, 1};
        const float off[3] = {0.1f, 0.0f, -0.2f};
        wrist_position(grip, ident, off, 0.06f, o);
        check(near3(o, 1.1f, 2.06f, 2.8f), "identity grip: offset adds, lift is +Y");
        float yaw90[4]; dvr::xrmath::quat_axis_angle(0, 1, 0, 3.14159265f * 0.5f, yaw90);
        wrist_position(grip, yaw90, off, 0.0f, o);
        // R_y(90): x' = z, z' = -x  ->  (0.1, 0, -0.2) -> (-0.2, 0, -0.1)
        check(near3(o, 1.0f - 0.2f, 2.0f, 3.0f - 0.1f), "a 90 degree yaw grip rotates the offset with it");
    }
    // The hide rules.
    {
        // toHead = head - panel. The head looks along -Z, so a panel AHEAD of
        // the eyes has a more negative z than the head: toHead.z is POSITIVE.
        const float fwd[3] = {0, 0, -1};
        const float ahead[3] = {0, 0.3f, 0.5f};        // the panel is 0.5 m ahead, 0.3 m below
        check(!behind_face(ahead, fwd), "a panel 0.5 m ahead is not behind the face");
        const float behind[3] = {0, 0, -0.3f};         // the panel is 0.3 m BEHIND the head
        check(behind_face(behind, fwd), "a panel behind the head is hidden");
        const float grazing[3] = {0, 0, 0.04f};        // 4 cm ahead: at the face
        check(behind_face(grazing, fwd), "a panel 4 cm ahead is at the face");
        const float atEye[3] = {0.02f, 0.02f, -0.02f};
        check(too_near(atEye), "a panel at the eye is hidden");
        check(!too_near(ahead), "a panel 0.5 m away is not too near");
        const float overhead[3] = {0, -1.0f, -0.05f};  // the panel almost straight above... head - panel = (0,-1,..): panel above the head
        check(billboard_degenerate(overhead), "a panel nearly overhead degenerates the billboard");
        const float side[3] = {0.5f, -0.3f, -0.5f};
        check(!billboard_degenerate(side), "an ordinary panel does not");
        // Within 11.5 degrees of vertical: sin(11.5 deg) = 0.199 < 0.2 -> degenerate; 12 deg -> not.
        const float deg11[3] = {std::sin(11.0f * 3.14159265f / 180.0f), -std::cos(11.0f * 3.14159265f / 180.0f), 0.0f};
        const float deg12[3] = {std::sin(12.0f * 3.14159265f / 180.0f), -std::cos(12.0f * 3.14159265f / 180.0f), 0.0f};
        check(billboard_degenerate(deg11), "11 degrees off vertical: degenerate");
        check(!billboard_degenerate(deg12), "12 degrees off vertical: fine");
    }
    // Crops.
    {
        const float full[4] = {0, 0, 1, 1};
        Crop c = crop_rect(1920, 1080, full, 1.0f, 0.0f);
        check(c.x == 0 && c.y == 0 && c.w == 1920 && c.h == 1080, "height 0: the whole texture");
        check(std::fabs(c.heightM - 1080.0f / 1920.0f) < 1e-5f, "height 0: the texture's aspect");
        c = crop_rect(1920, 1080, full, 1.0f, 1.0f);
        check(c.w == 1080 && c.h == 1080 && c.x == 420 && c.y == 0, "a 1:1 crop of 16:9 trims the sides to 1080 wide at x=420");
        check(std::fabs(c.widthM - 1.0f) < 1e-6f && std::fabs(c.heightM - 1.0f) < 1e-6f, "the asked metres are kept");
        c = crop_rect(1000, 2000, full, 1.0f, 1.0f);
        check(c.w == 1000 && c.h == 1000 && c.y == 500, "a 1:1 crop of a tall texture trims top and bottom");
        const float sub[4] = {0.25f, 0.5f, 0.75f, 1.0f};
        c = crop_rect(2000, 1000, sub, 0.5f, 0.0f);
        check(c.x == 500 && c.y == 500 && c.w == 1000 && c.h == 500, "a sub-rectangle maps to pixels");
        check(std::fabs(c.heightM - 0.25f) < 1e-6f, "the sub-rectangle's own aspect");
        const float bad[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        c = crop_rect(100, 100, bad, 1.0f, 0.0f);
        check(c.w == 100 && c.h == 100, "an empty sub-rectangle falls back to the whole texture");
    }
    {
        const float r[4]={.52f,.46f,.80f,.62f};
        float width=.28f,offset[2]={.16f,-.04f};
        expand_reference_panel(r,1,width,offset);
        check(std::fabs(width-1)<1e-6f,"full private texture preserves prompt pixel scale");
        check(std::fabs(offset[0])<1e-6f && std::fabs(offset[1])<1e-6f,"reference window position preserved");
        width=.40f;offset[0]=offset[1]=0;
        expand_reference_panel(r,1,width,offset);
        check(std::fabs(offset[0]+.16f*width)<1e-6f,"hand crop remains centered at grip after expansion");
        check(std::fabs(offset[1]-.04f*width)<1e-6f,"hand crop vertical position preserved");
        // A point outside the old prompt rectangle still has a stable mapping.
        check(std::fabs((offset[0]+(.9f-.5f)*width)-(.9f-.66f)*width)<1e-6f,"moving content is not clipped at old region edge");
    }
    {
        OpeningOrientation vertical;
        const float down[4]={-.5f,0,0,.8660254f};
        check(vertical.capture_upright(down) && std::fabs(vertical.q[0])<.00001f && std::fabs(vertical.q[2])<.00001f,
              "looking down to open a reader cannot tilt it");
        const float upAxis[3]={0,1,0};float pageUp[3];
        dvr::xrmath::quat_rotate(vertical.q[0],vertical.q[1],vertical.q[2],vertical.q[3],upAxis,pageUp);
        check(std::fabs(pageUp[1]-1)<.00001f,"reader up axis is vertical");
        OpeningOrientation opening;const float first[4]={0,0,0,1},turned[4]={0,.7071068f,0,.7071068f};
        check(opening.capture(first) && opening.capture(turned) && opening.q[3]==1,"opening rotation cannot swivel with head");
        opening.reset();check(opening.capture(turned) && opening.q[1]>.70f,"next opening takes new rotation");
        const float grip[3]={.1f,.2f,-.4f};float pos[3];
        camera_panel_position(grip,opening.q,.1f,pos);
        check(std::fabs(pos[0])<.00001f && std::fabs(pos[1]-.2f)<.00001f,"reader offset uses opening axes while position follows hand");
        using namespace dvr::hudmarker;
        Delivery delivery;const float a[4]={.1f,.2f,.133f,.232f},b[4]={.8f,.2f,.833f,.232f};
        delivery.drawing.add(a);delivery.copied(0,true);
        delivery.drawing.add(b);delivery.copied(1,true);delivery.delivered(0);
        check(delivery.output.count==1 && delivery.output.rect[0][0]==a[0],"delayed image gets its own marker bounds");
        delivery.copied(0,true);delivery.delivered(0);check(delivery.output.count==0,"empty image cannot retain old markers");
        delivery.drawing.add(a);delivery.copied(1,false);delivery.delivered(1);
        check(delivery.output.rect[0][0]==b[0],"failed copy retains old image metadata");
        delivery.reset();check(delivery.output.count==0 && delivery.slot[1].count==0,"device reset clears metadata");
        Regions regions;regions.add(a);regions.add(a);check(regions.count==1,"duplicate marker draw has one crop");
        for(int i=0;i<kMax+1;++i) {float r[4]={i*.12f,0,i*.12f+.03f,.03f};regions.add(r);}
        check(regions.overflow,"marker budget overflow requests complete panel");
        regions=Regions{};regions.add(a);const float overlap[4]={.11f,.2f,.143f,.232f};regions.add(overlap);
        check(regions.overflow,"overlapping marker pixels use complete panel");
        regions=Regions{};regions.add(a);regions.add(b);
        check(separable(regions,1506,1561),"distant markers have independent crops");
        const float close[4]={.134f,.2f,.167f,.232f};regions.add(close);
        check(!separable(regions,1506,1561),"padded crop cannot duplicate adjacent marker pixels");
        float offset[2],small=0,big=0;
        check(placement(b,1.3f,1.257f,1.3f,1.25f,offset,small),"valid marker projection");
        const float x=offset[0];placement(b,1.3f,1.257f,1.3f,2.5f,offset,big);
        check(offset[0]==x && std::fabs(big-2*small)<.00001f,"icon scale does not change tracking position");
        check(x>.9f,"marker travel reaches rendered frustum beyond small panel");
        float crop[4],paddedOff[2],paddedWidth;
        check(cropped_placement(b,1506,1561,1.3f,1.257f,1.3f,1.25f,crop,paddedOff,paddedWidth),"padded marker placement valid");
        const float iconCenter=paddedOff[0]+((b[0]+b[2]-crop[0]-crop[2])*.5f)*1.25f;
        check(std::fabs(iconCenter-x)<.000001f,"crop padding cannot shift projected marker center");
        check(!placement(b,0,1,1,1,offset,small),"invalid projection falls back");
        for(int i=0;i<200;++i) {
            float r[4]={i*.0048f,.2f,i*.0048f+.033f,.232f},crop[4];padded_crop(r,1506,1561,crop);
            auto c=crop_rect(1506,1561,crop,1,0);
            check(c.w==64 && c.h==64 && c.x>=0 && c.x+c.w<=1506,"marker crop size stable and in texture across motion");
            check(crop[0]<=r[0] && crop[2]>=r[2],"padded crop retains marker pixels");
        }
    }
    {
        GripPanel p;const float initial[4]={0,0,0,1},grip[4]={0,.70710678f,0,.70710678f};float q[4];
        check(p.orient(grip,initial,q) && fabsf(q[3]-1)<.0001f,"grip attachment preserves initial vertical placement");
        const float next[4]={0,1,0,0};
        check(p.orient(next,initial,q) && fabsf(q[1]-.70710678f)<.0001f,"page follows grip rotation delta");
        float pos[3],hp[3]={1,2,3};camera_panel_position(hp,q,.2f,pos);
        check(fabsf(pos[0]-.8f)<.0001f && fabsf(pos[2]-3)<.0001f,"page depth offset rotates rigidly with grip");
        p.reset();check(p.orient(next,initial,q) && fabsf(q[3]-1)<.0001f,"reopening recaptures initial pose");
    }
    {
        const float identity[]={0,0,0,1},yaw[]={0,.70710678f,0,.70710678f};float out[4];
        reading_tilt(yaw,0,out);
        for(int k=0;k<4;++k)check(fabsf(out[k]-yaw[k])<.0001f,"zero tilt retains existing attachment");
        reading_tilt(identity,90,out);check(fabsf(out[0]-.70710678f)<.0001f && !out[1] && !out[2],"slider changes only pitch");
        reading_tilt(identity,-90,out);check(fabsf(out[0]+.70710678f)<.0001f,"negative tilt reverses pitch");
        reading_tilt(yaw,90,out);check(fabsf(out[0]-.5f)<.0001f && fabsf(out[1]-.5f)<.0001f && fabsf(out[2]+.5f)<.0001f,"tilt is local to attached page");
    }
    {
        const float grip[]={.672240f,-.104614f,-.336110f,.651290f};
        const float recorded[]={-.049763f,-.026783f,.011967f,.998330f};
        float place[4],page[4],expectedPlace[4];
        check(reading_grip_reference(grip,place,page),"recorded grip accepted");
        reading_tilt(recorded,45.267f,expectedPlace);
        for(int k=0;k<4;++k){
            check(fabsf(page[k]-recorded[k])<.00001f,"reproduce recorded comfortable page");
            check(fabsf(place[k]-expectedPlace[k])<.00001f,"reproduce placement independently of page pitch");
        }
        for(int i=0;i<30;++i){
            float turn[4],moved[4],p[4],o[4],expected[4],expectedP[4];
            dvr::xrmath::quat_axis_angle(0,1,0,i*.1f,turn);
            dvr::xrmath::quat_mul(turn,grip,moved);
            check(reading_grip_reference(moved,p,o),"rotated grip accepted");
            dvr::xrmath::quat_mul(turn,page,expected);
            dvr::xrmath::quat_mul(turn,place,expectedP);
            for(int k=0;k<4;++k){
                check(fabsf(o[k]-expected[k])<.00001f,"page rotates rigidly with hand");
                check(fabsf(p[k]-expectedP[k])<.00001f,"placement rotates rigidly with hand");
            }
            check(reading_grip_reference(grip,p,o),"return to reference after different initial hand poses");
            for(int k=0;k<4;++k)check(fabsf(o[k]-page[k])<.00001f,"no opening history changes reference");
        }
        const float zero[]={0,0,0,0},bad[]={NAN,0,0,1};
        check(!reading_grip_reference(zero,place,page),"invalid zero grip refused");
        check(!reading_grip_reference(bad,place,page),"nonfinite grip refused");
    }
    {
        check(reading_trim(-31,false)==0,"accepted old trim becomes zero");
        check(reading_trim(0,true)==0,"new zero remains zero");
        check(reading_trim(10,false)==41,"legacy custom angle preserved");
        check(reading_trim(180,false)==-149,"legacy edge angle wraps without changing physical rotation");
        const float q[]={0,0,0,1};float old[4],updated[4];
        reading_tilt(q,-31,old);reading_alignment(q,0,updated);
        for(int k=0;k<4;++k)check(fabsf(old[k]-updated[k])<.000001f,"rebase retains accepted physical orientation");
    }
    std::printf("%u hud-anchor checks passed\n", checks);
    return 0;
}
