#pragma once
#include <string>
namespace dvr::setup::support {
// The GUI and headless regression lane invoke this exact embedded collector.
bool collect(const std::wstring& gameDir, const std::wstring& outDir, bool openFolder, std::string* notice);
}
