#pragma once

#include <cstdio>

// 디버그 빌드에서만 동작하는 로그 매크로. 릴리즈 빌드에서는 완전히 제거된다.
#ifdef _DEBUG
#define DEBUG_LOG(fmt, ...) printf(fmt "\n", __VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif
