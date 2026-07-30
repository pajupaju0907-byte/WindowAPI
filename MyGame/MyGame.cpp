#include "pch.h"

// MyGame.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "framework.h"
#include "MyGame.h"
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

    if (!WindowManager::GetInstance().Init(hInstance, nCmdShow))
    {
        return FALSE;
    }
    SceneManager::GetInstance().ChangeScene(SceneType::Play);
    InvalidateRect(WindowManager::GetInstance().GetWindowHandle(), nullptr, FALSE);

    int result = WindowManager::GetInstance().MessageLoop();

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return result;
}
