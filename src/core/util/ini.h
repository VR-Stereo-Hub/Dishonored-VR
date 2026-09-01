// core/util/ini.h - small file helpers around the Win32 private-profile API.
#pragma once

namespace dvr::ini {
float read_float(const char* ini, const char* section, const char* key, float def);
int   read_int(const char* ini, const char* section, const char* key, int def);
bool  write_text_file(const char* path, const char* text);
} // namespace dvr::ini

// Original names.
inline float IniFloat(const char* ini, const char* sec, const char* key, float def)
{ return ::dvr::ini::read_float(ini, sec, key, def); }
inline bool WriteTextFile(const char* path, const char* text)
{ return ::dvr::ini::write_text_file(path, text); }
