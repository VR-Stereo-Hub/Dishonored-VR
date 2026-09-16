// Included in the overlay and its standalone production-control regression.
static void OverlayLegacyFovControl()
{
    bool on=g_fovLever>=40.0f;
    if (ImGui::Checkbox("FOV lever (force the game's rendered FOV)",&on)) {
        g_fovLever=on?95.0f:0.0f;
        dvr::camera::set_fov_deg(g_fovLever);
    }
    if (on) {
        float lever=g_fovLever;
        if (ImGui::SliderFloat("  target FOV (deg)",&lever,60.0f,140.0f,"%.0f")) {
            g_fovLever=lever;
            dvr::camera::set_fov_deg(g_fovLever);
        }
        ImGui::TextDisabled("  the lever writes the game camera's FOV every dispatch");
    }
}
