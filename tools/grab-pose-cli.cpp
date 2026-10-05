// The production grab maths (src/game/dishonored/hands/grab_pose.h) behind a line protocol, so an
// offline check (tools/blender/grab_verify.py) drives the same code the game runs instead of a
// copy of it. Built by tools\grab-pose-host.ps1. Reads stdin, one request a line:
//   T <shape> <close> <hold> <release> <lag>      set the timing (ms)            -> "ok"
//   P <elapsed> <releaseAt> <along>               phase and this bone's progress -> "<phase> <s>"
//   B <s> <a0..a11> <b0..b11>                     screw blend of two 3x4         -> 12 floats
//   L <s> <a0..a11> <b0..b11>                     plain element-wise blend (the comparison) -> 12 floats
#include <cstdio>
#include <cstring>
#include "game/dishonored/hands/grab_pose.h"

int main()
{
    using namespace dvr::grab;
    Timing T = { 70, 220, 180, 260, 40 };
    char line[2048];
    while (std::fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'T') {
            std::sscanf(line + 1, "%f %f %f %f %f", &T.shapeMs, &T.closeMs, &T.holdMs, &T.releaseMs, &T.lagMs);
            std::puts("ok");
        } else if (line[0] == 'P') {
            float e = 0, r = -1, a = 0;
            std::sscanf(line + 1, "%f %f %f", &e, &r, &a);
            const State st = phase_at(T, e, r);
            float s = st.s;
            if (st.phase == kClose) s = per_bone_close(T, st.s, a);
            std::printf("%d %.6f\n", (int)st.phase, s);
        } else if (line[0] == 'J') {                     // J <w> <Q 12> <cenP 3> <cenC 3> -> score residual
            float v[19], w = 0; const char* p = line + 1; int n = 0; bool ok = std::sscanf(p, "%f%n", &w, &n) == 1;
            p += n;
            for (int i = 0; i < 18 && ok; i++) { if (std::sscanf(p, "%f%n", &v[i], &n) != 1) ok = false; else p += n; }
            if (!ok) { std::puts("err"); continue; }
            std::printf("%.6f %.6f\n", parent_score(v, v + 12, v + 15, w), joint_residual(v, v + 12, v + 15));
        } else if (line[0] == 'B' || line[0] == 'L') {
            float s = 0, A[12], B[12], O[12];
            const char* p = line + 1; int n = 0;
            if (std::sscanf(p, "%f%n", &s, &n) != 1) { std::puts("err"); continue; }
            p += n;
            bool ok = true;
            for (int i = 0; i < 24 && ok; i++) { float v; if (std::sscanf(p, "%f%n", &v, &n) != 1) ok = false; else { (i < 12 ? A[i] : B[i - 12]) = v; p += n; } }
            if (!ok) { std::puts("err"); continue; }
            if (line[0] == 'B') screw_blend_3x4(A, B, s, O);
            else for (int i = 0; i < 12; i++) O[i] = A[i] + s * (B[i] - A[i]);
            for (int i = 0; i < 12; i++) std::printf(i ? " %.7g" : "%.7g", O[i]);
            std::putchar('\n');
        }
        std::fflush(stdout);
    }
    return 0;
}
