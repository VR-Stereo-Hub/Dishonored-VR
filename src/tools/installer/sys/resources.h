// tools/installer/sys/resources.h - the embedded payload (payload.rc).
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace dvr::setup::resources {

struct Blob { const uint8_t* data = nullptr; size_t size = 0; bool ok() const { return data && size; } };
// The bytes live in the image mapping for the life of the process; never freed.
Blob rcdata(int id);

} // namespace dvr::setup::resources
