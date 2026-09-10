// Compile the production writer and clamp functions with a small camera buffer.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
static bool RangeReadable(const void* p, size_t) { return p != nullptr; }
#include "camera_clamp_body.inc"

static int failed, checks;
static void check(const char* name, bool ok) {
    ++checks;
    std::printf("camera-clamp/%s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failed;
}
static bool near(float a, float b) { return std::fabs(a-b) < .001f; }
struct Fixture {
    float data[6] = {10, 20, 100, 10, 20, 100};
    Writer w;
    uint8_t* cam() { return reinterpret_cast<uint8_t*>(data); }
    void eye(float x, float y, float z = 0) {
        float off[3] = {x, y, z};
        write_offset(cam(), 0, off, w, nullptr);
    }
};

int main() {
    Fixture a;
    a.eye(-3.155f, 0);
    clamp_written_z(a.cam(), 0, 99, &a.w);
    float base[3]; current_base(a.cam(), 0, a.w, base);
    check("clamp_retains_original_base", near(base[0],10) && near(base[2],100));
    a.eye(3.155f, 0, -1);
    check("opposite_eye_after_downward_clamp", near(a.data[0],13.155f) && near(a.data[2],99));

    Fixture b;
    for (int n=0; n<100; ++n) {
        b.eye(-3.155f, 1.2f, -float(n));
        clamp_written_z(b.cam(), 0, 99-float(n), &b.w);
    }
    current_base(b.cam(), 0, b.w, base);
    check("repeated_descent_does_not_accumulate_xy", near(b.data[0],6.845f) && near(b.data[1],21.2f));
    check("repeated_descent_retains_base_z", near(base[2],100));
    restore(b.cam(), 0, b.w);
    check("release_removes_all_mod_offsets", near(b.data[0],10) && near(b.data[1],20) && near(b.data[2],100));

    Fixture c; c.eye(1.1f, -2.9f, .7f);
    clamp_written_z(c.cam(), 0, 97, &c.w);
    c.eye(-1.1f, 2.9f, -3);
    check("rolled_eye_and_lean_keep_base", near(c.data[0],8.9f) && near(c.data[1],22.9f) && near(c.data[2],97));

    Fixture d; d.eye(-3.155f, 0);
    d.data[2] = 95; // Engine changed Z before the mod clamp: not an owned write.
    check("engine_z_change_is_not_adopted", !clamp_written_z(d.cam(),0,94,&d.w));
    current_base(d.cam(),0,d.w,base);
    check("engine_z_change_remains_fresh", near(base[0],6.845f) && near(base[2],94));

    Fixture e; e.eye(-3.155f,0);
    e.data[0]=50; e.data[1]=60; e.data[2]=80;
    check("engine_recompute_is_not_adopted", !clamp_written_z(e.cam(),0,79,&e.w));
    e.eye(3.155f,0,-1);
    check("engine_recompute_uses_new_base", near(e.data[0],53.155f) && near(e.data[1],60) && near(e.data[2],78));

    Fixture f, other; f.eye(-3.155f,0);
    std::memcpy(other.data,f.data,12);
    check("different_camera_not_adopted", !clamp_written_z(other.cam(),0,99,&f.w));
    check("different_camera_does_not_change_ledger", near(f.w.last[2],100) && near(f.w.lastOff[2],0));
    std::memcpy(f.data+3,f.data,12);
    check("different_field_not_adopted", !clamp_written_z(f.cam(),12,98,&f.w));
    check("other_field_still_clamped", near(f.data[5],98) && near(f.w.last[2],100));

    Fixture g; g.eye(-3.155f,0);
    check("upward_ceiling_leaves_field_unchanged", !clamp_written_z(g.cam(),0,110,&g.w) && near(g.data[2],100));
    g.eye(3.155f,0);
    check("unclamped_eye_pair_unchanged", near(g.data[0],13.155f));
    Fixture h;
    check("no_writer_still_clamps", !clamp_written_z(h.cam(),0,90,nullptr) && near(h.data[2],90));
    check("nan_ceiling_is_noop", !clamp_written_z(h.cam(),0,std::numeric_limits<float>::quiet_NaN(),nullptr) && near(h.data[2],90));
    check("null_camera_is_noop", !clamp_written_z(nullptr,0,90,&h.w));
    std::printf("camera-clamp: %d checks, %d failures\n", checks, failed);
    return failed ? 1 : 0;
}
