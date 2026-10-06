// Host stub for tools/embedded-assets-host.ps1: the production log macros as printf.
#pragma once
#include <stdio.h>
namespace dvr::log { enum class Cat { core }; enum class Level { Info, Warn }; }
#define DVR_INFO(...) (printf("I " __VA_ARGS__), printf("\n"))
#define DVR_WARN(...) (printf("W " __VA_ARGS__), printf("\n"))
