// core/vr/xr_loader_log.h - the OpenXR loader's own words in the mod's log.
//
// The statically linked loader explains a failed xrCreateInstance - which
// layer it skipped, which library would not load and the Win32 error - but
// only to stderr (a GUI game has none), the debugger, or stdout behind
// XR_LOADER_DEBUG. None of that reaches a support bundle, so a remote failure
// arrived as a bare XrResult(-32). install() adds one recorder to the loader's
// logger that writes its warnings and errors, and its layer-related info
// lines, to the mod log as `xr/loader:` lines. It must run before the first
// loader call; it is idempotent.
#pragma once

namespace dvr::xr_loader_log {

void install();

// Lines forwarded so far (warnings + errors), for the failure explainer.
unsigned warnings_seen();

} // namespace dvr::xr_loader_log
