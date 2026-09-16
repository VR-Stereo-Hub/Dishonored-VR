#pragma once
namespace dvr::log { enum class Cat { perf, core }; enum class Level { Info, Warn }; }
void test_log(const char*,...);
#define DVR_LOG(cat,level,...) test_log(__VA_ARGS__)
#define DVR_INFO(...) test_log(__VA_ARGS__)
#define DVR_ERROR(...) test_log(__VA_ARGS__)
