// core/util/diag.h - DVR_SKIP: comma-separated subsystem tokens to leave
// uninstalled, e.g. DVR_SKIP=hands,overlay. A bisect tool for "which of our
// subsystems breaks this machine" with no rebuild per step. Unset = everything
// installs; only install paths read it.
#pragma once
#include <string.h>
#include <stdlib.h>

namespace dvr::diag {
inline bool skip(const char* token)
{
    const char* list = getenv("DVR_SKIP");
    if (!list || !*list) return false;
    const size_t n = strlen(token);
    for (const char* p = list; *p;) {
        const char* end = strchr(p, ',');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == n && strncmp(p, token, n) == 0) return true;
        if (!end) break;
        p = end + 1;
    }
    return false;
}
} // namespace dvr::diag
