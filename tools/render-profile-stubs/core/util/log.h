#pragma once
namespace dvr::log { enum class Cat { perf }; enum class Level { Info }; }
void test_log(const char*,...);
#define DVR_LOG(cat,level,...) test_log(__VA_ARGS__)
