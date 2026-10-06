// Host stub for tools/embedded-assets-host.ps1: the data directory is the test's working directory.
#pragma once
#include <stdio.h>
namespace dvr::paths { inline const char* in_data_dir(char* out, const char* name) { sprintf(out, ".\data\%s", name); return out; } }
