// tools/installer/sys/resources.h - the embedded payload (payload.rc).
#pragma once
#include <stdint.h>
#include <stddef.h>
struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace dvr::setup::resources {

struct Blob { const uint8_t* data = nullptr; size_t size = 0; bool ok() const { return data && size; } };
// The bytes live in the image mapping for the life of the process; never freed.
Blob rcdata(int id);
// Caller owns the returned view. WIC decodes embedded PNGs without disk files.
ID3D11ShaderResourceView* image(ID3D11Device* device, int id, unsigned* width, unsigned* height);

} // namespace dvr::setup::resources
