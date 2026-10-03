// core/vr/xr_loader_log.cpp - see xr_loader_log.h.
//
// This reaches into the loader's internal logger (loader_logger.hpp), which is
// only possible because the loader is compiled into this DLL from the vendored
// SDK. No loader file is edited: the recorder is an ordinary LoaderLogRecorder,
// the same kind the loader adds for stderr and the debugger.
#include "core/vr/xr_loader_log.h"

#include <windows.h>
#include <atomic>
#include <cstring>
#include <memory>

#include "core/util/log.h"

#if DVR_WITH_OPENXR
#include <openxr/openxr.h>
#include "loader_logger.hpp"

namespace {

std::atomic<unsigned> g_warnings{0};
std::atomic<unsigned> g_layerInfo{0};

// Bounded on purpose: the loader can repeat itself on every pre-instance call,
// and a launch that retries the shim makes two passes. Enough for both passes
// of a failure, never a flood.
constexpr unsigned kMaxWarnings = 64;
constexpr unsigned kMaxLayerInfo = 32;

class ModLogRecorder : public LoaderLogRecorder {
   public:
    ModLogRecorder()
        : LoaderLogRecorder(XR_LOADER_LOG_DEBUGGER, nullptr,
                            XR_LOADER_LOG_MESSAGE_SEVERITY_INFO_BIT |
                                XR_LOADER_LOG_MESSAGE_SEVERITY_WARNING_BIT |
                                XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT,
                            0xFFFFFFFFUL) {
        Start();
    }

    bool LogMessage(XrLoaderLogMessageSeverityFlagBits severity, XrLoaderLogMessageTypeFlags,
                    const XrLoaderLogMessengerCallbackData* d) override {
        if (!d || !d->message) return false;
        const char* cmd = (d->command_name && d->command_name[0]) ? d->command_name : "-";
        if (severity & (XR_LOADER_LOG_MESSAGE_SEVERITY_WARNING_BIT |
                        XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT)) {
            // Printed on every machine with no EXPLICIT layer registered (the
            // usual case) - measured on the dev PC. Not a fault; dropped so it
            // cannot be read as one.
            if (strstr(d->message, "failed to read registry location") &&
                strstr(d->message, "Explicit"))
                return false;
            if (g_warnings.fetch_add(1) >= kMaxWarnings) return false;
            DVR_LOG(::dvr::log::Cat::openxr, ::dvr::log::Level::Warn, "xr/loader: %s [%s] %s",
                    (severity & XR_LOADER_LOG_MESSAGE_SEVERITY_ERROR_BIT) ? "ERROR" : "warning",
                    cmd, d->message);
            return false;
        }
        // Info is mostly the loader narrating its search. Only the lines about
        // API layers are kept: "Implicit layer ... is disabled" is the proof
        // that an opt-out reached the loader, and "succeeded loading layer"
        // names every layer that is actually in the call chain.
        if (!strstr(d->message, "layer") && !strstr(d->message, "Layer")) return false;
        if (g_layerInfo.fetch_add(1) >= kMaxLayerInfo) return false;
        DVR_LOG(::dvr::log::Cat::openxr, ::dvr::log::Level::Info, "xr/loader: [%s] %s", cmd,
                d->message);
        return false;
    }
};

} // namespace

namespace dvr::xr_loader_log {

void install() {
    static std::atomic<bool> done{false};
    if (done.exchange(true)) return;
    LoaderLogger::GetInstance().AddLogRecorder(std::unique_ptr<LoaderLogRecorder>(new ModLogRecorder()));
    DVR_LOG(::dvr::log::Cat::openxr, ::dvr::log::Level::Info,
            "xr/loader: the loader's warnings, errors and API-layer lines are recorded here as "
            "'xr/loader:' lines. A failed xrCreateInstance with NO such line above it was not "
            "explained by the loader.");
}

unsigned warnings_seen() { return g_warnings.load(); }

} // namespace dvr::xr_loader_log

#else

namespace dvr::xr_loader_log {
void install() {}
unsigned warnings_seen() { return 0; }
} // namespace dvr::xr_loader_log

#endif
