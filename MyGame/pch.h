#pragma once

// 프로젝트 전체 .cpp가 공통으로 필요로 하는, 자주 바뀌지 않는 시스템 헤더 모음.
// 미리 컴파일된 헤더(Precompiled Header)이므로 모든 .cpp 파일의 첫 줄은 반드시 #include "pch.h"여야 한다.

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <string>
// 렌더링은 Direct2D(+ 텍스트용 DirectWrite, 이미지 디코딩용 WIC)를 쓴다.
// ComPtr(<wrl/client.h>)로 COM 객체 소유권을 RAII로 관리해서 Release()를 직접 호출하지 않게 한다.
#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")
#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#include <wrl/client.h>

using namespace std;