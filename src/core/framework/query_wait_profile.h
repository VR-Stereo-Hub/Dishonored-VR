#pragma once
#include <stdint.h>
#include <stddef.h>
namespace dvr::query_profile {
void configure(bool armed); // launch-only hook installation, default off
void set_enabled(bool on); // live diagnostic toggle, no engine behavior change
bool enabled();
bool armed();
void install(uintptr_t target, const uint8_t* prefix, uintptr_t moduleBase, size_t signatureLength = 6);
void end_frame(int drawnEye, bool gameplay); // render thread; interval ends at this draw
} // namespace dvr::query_profile
