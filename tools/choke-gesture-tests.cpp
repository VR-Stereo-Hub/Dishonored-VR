// tools/choke-gesture-tests.cpp - VR-145's gesture on the host. Build and run:
//   powershell -File tools/choke-gesture-host.ps1
#include "game/dishonored/choke_gesture.h"
#include <cstdio>
#include <cstdlib>
static int checks = 0;
static void check(bool v, const char* why) { ++checks; if (!v) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); } }
int main() {
    using namespace dvr::choke;
    Config c; c.enabled = true;
    const float head[3] = {0, 1.6f, 0}, ident[4] = {0, 0, 0, 1};
    float s[3]; shoulder_point(c, head, ident, s);
    check(s[0] < -0.1f && s[1] < 1.5f && s[2] > 0, "facing -Z: the left shoulder is to -X, below, slightly behind");
    // A yawed head carries the shoulder around it: 90 degrees left (about +Y).
    const float yawL[4] = {0, 0.70710678f, 0, 0.70710678f};
    float s2[3]; shoulder_point(c, head, yawL, s2);
    check(s2[2] > 0.1f, "turned left (facing -X), the left shoulder moves to +Z");
    // A pitched head does not move it.
    const float pitch[4] = {0.38268343f, 0, 0, 0.92387953f};
    float s3[3]; shoulder_point(c, head, pitch, s3);
    check(std::fabs(s3[0]-s[0]) < 1e-4f && std::fabs(s3[2]-s[2]) < 1e-4f, "a nod does not swing the shoulder");
    // A slow drift into the zone does not start; a quick move does; leaving releases.
    State st; int e = 0; double t = 0;
    float far[3] = {0.3f, 1.2f, -0.4f};
    st.update(c, t, far, head, ident, true, &e);
    float slow[3];
    for (int i = 1; i <= 100; ++i) {   // 2 s at 50 Hz, walking in at well under MinSpeed
        const float a = i / 100.0f;
        for (int k = 0; k < 3; ++k) slow[k] = far[k] + (s[k] - far[k]) * a;
        t += 20; st.update(c, t, slow, head, ident, true, &e);
    }
    check(!st.held, "a slow drift into the shoulder zone does not start a choke");
    State q; q.update(c, 0, far, head, ident, true, &e);
    q.update(c, 100, s, head, ident, true, &e);   // ~0.8 m in 100 ms = fast
    check(q.held && e == 1, "a quick move into the zone starts it");
    float near[3] = {s[0] + 0.15f, s[1], s[2]};
    q.update(c, 400, near, head, ident, true, &e);
    check(q.held, "held while within the release distance");
    float away[3] = {s[0] + 0.30f, s[1], s[2]};
    q.update(c, 700, away, head, ident, true, &e);
    check(!q.held && e == -1, "moving the hand away releases it");
    q.update(c, 800, far, head, ident, true, &e);
    q.update(c, 900, s, head, ident, false, &e);
    check(!q.held, "not allowed (menu/wheel) never starts");
    Config off; State o; o.update(off, 0, far, head, ident, true, &e); o.update(off, 100, s, head, ident, true, &e);
    check(!o.held, "Gesture=0 never starts");
    std::printf("%d choke-gesture checks passed\n", checks);
    return 0;
}
