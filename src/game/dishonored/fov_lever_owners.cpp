// VR-213: persistent FOV/eye-clamp writers share the camera's current ownership.
// This definition follows the identity helpers; fov_lever.cpp calls via fwd.h.
static bool FovLeverOwnersReady() {
    static CtIdentity owners[2];
    static bool have = false;
    static LONG load = -1;
    static unsigned epoch = 0;
    static double retry = 0;
    const unsigned currentEpoch = UiSurfaceEpoch();
    const auto valid = [&]() {
        return have && load == g_mkLoadEvents && ChSlot(owners[0]) && ChSlot(owners[1]) &&
            owners[0].value.obj == g_camObj && owners[1].value.obj == g_peCtrl &&
            CtObject(g_peCtrl, g_ctPcCamera) == g_camObj;
    };
    if (epoch == currentEpoch && valid()) return true;
    const double now = MaimNowMs();
    if (now < retry) return false;
    retry = now + 1000;
    if (!BuildLiveSet()) {
        Log("fovlever: refused: live-object table refresh failed");
        return false;
    }
    if (!g_ctPcCamera && !FindPropOffsetChecked("PlayerController", "PlayerCamera", &g_ctPcCamera)) {
        Log("fovlever: refused: controller camera ownership unavailable");
        return false;
    }
    // Only keep the baseline when the old identities are proven live again.
    const bool same = valid();
    CtIdentity next[2];
    if (!IsLiveObject(g_peCtrl) || !IsLiveObject(g_camObj) ||
        CtObject(g_peCtrl, g_ctPcCamera) != g_camObj ||
        !ChCapture(g_camObj, &next[0]) || !ChCapture(g_peCtrl, &next[1])) {
        have = false;
        g_fovNatural = 0;
        Log("fovlever: refused: live camera/controller ownership mismatch");
        return false;
    }
    if (!same) g_fovNatural = 0;
    owners[0] = next[0]; owners[1] = next[1];
    have = true; load = g_mkLoadEvents; epoch = currentEpoch; retry = 0;
    Log("fovlever: owners revalidated load=%ld UI epoch=%u baseline=%s", load, epoch, same ? "retained" : "recapture");
    return true;
}
