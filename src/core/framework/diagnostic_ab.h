// VR-124: temporary read-only collector suppression. Never changes saved settings.
#pragma once
#include <stdint.h>
namespace dvr::diag_ab {
bool enabled();
bool reduced();
void set_enabled(bool);
void tick(bool gameplay);
void submit(bool stereo,uint32_t left,uint32_t right);
void invalidate(); // any lane: request abort at next present, no shared state mutation
}
