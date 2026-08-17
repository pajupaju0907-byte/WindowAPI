#include "pch.h"

// MyGame.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "framework.h"
#include "MyGame.h"
#include "src/managers/ResourceManager.h"
#include "src/managers/SoundManager.h"
#include "src/core/WindowManager.h"
#include "src/core/SceneManager.h"
#include <cstdlib>
#include <ctime>
#include "src/managers/RankingManager.h"

// 창 생성/메시지 루프는 WindowManager(싱글톤)가 전담한다.
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    srand(static_cast<unsigned int>(time(nullptr)));

    // WIC(스프라이트 PNG 디코딩)가 COM을 쓰므로, WindowManager::Init()에서 WIC 팩토리를 만들기 전에
    // 이 스레드에 COM을 초기화해둬야 한다
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        return FALSE;
    }

    if (!WindowManager::GetInstance().Init(hInstance, nCmdShow))
    {
        CoUninitialize();
        return FALSE;
    }
    RankingManager::GetInstance().Init("https://wobble-block-default-rtdb.firebaseio.com/");

    SceneManager::GetInstance().ChangeScene(SceneType::Title);
    InvalidateRect(WindowManager::GetInstance().GetWindowHandle(), nullptr, FALSE);

    int result = WindowManager::GetInstance().MessageLoop();

    // Direct2D 비트맵(ComPtr)들이 프로세스 종료 전에, COM이 아직 살아있는 상태에서 해제되도록
    // 명시적으로 정리한다
    ResourceManager::GetInstance().Shutdown();
    SoundManager::GetInstance().Shutdown();
    WindowManager::GetInstance().Shutdown();

    CoUninitialize();
    return result;
}
