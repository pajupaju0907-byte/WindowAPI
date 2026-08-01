#include "pch.h"

// MyGame.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "framework.h"
#include "MyGame.h"
#include "src/managers/ResourceManager.h"
#include "src/core/WindowManager.h"
#include "src/core/SceneManager.h"

// 창 생성/메시지 루프는 WindowManager(싱글톤)가 전담한다.
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // GDI+ 초기화 완료

    if (!WindowManager::GetInstance().Init(hInstance, nCmdShow))
    {
        return FALSE;
    }
    SceneManager::GetInstance().ChangeScene(SceneType::Play);
    InvalidateRect(WindowManager::GetInstance().GetWindowHandle(), nullptr, FALSE);

    int result = WindowManager::GetInstance().MessageLoop();

    // 모든 리소스를 명시적으로 해제하여 GDI+가 활성화된 상태에서
    // 비트맵들이 파괴되게 합니다.
    ResourceManager::GetInstance().Shutdown();

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return result;
}
