#pragma once

// 프로젝트 전체 .cpp가 공통으로 필요로 하는, 자주 바뀌지 않는 시스템 헤더 모음.
// 미리 컴파일된 헤더(Precompiled Header)이므로 모든 .cpp 파일의 첫 줄은 반드시 #include "pch.h"여야 한다.

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace std;