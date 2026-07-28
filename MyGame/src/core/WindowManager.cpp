#include "WindowManager.h"

WindowManager& WindowManager::GetInstance()
{
    static WindowManager instance;
    return instance;
}

bool WindowManager::Init(HINSTANCE hInstance, int nCmdShow)
{
    m_hInstance = hInstance;

    // TODO: 윈도우 클래스 등록(RegisterClassExW) 및 CreateWindowW 직접 구현
    (void)nCmdShow;
    return false;
}

int WindowManager::MessageLoop()
{
    // TODO: GetMessage/TranslateMessage/DispatchMessage 루프 직접 구현
    return 0;
}

HWND WindowManager::GetWindowHandle() const
{
    return m_hWnd;
}
