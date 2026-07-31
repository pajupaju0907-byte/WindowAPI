#pragma once

#include <windows.h>

// Win32 윈도우 생성과 메시지 루프를 담당하는 싱글톤
class WindowManager
{
public:
    static WindowManager& GetInstance();

    // 윈도우 클래스 등록 및 생성
    bool Init(HINSTANCE hInstance, int nCmdShow);

    // 기본 메시지 루프. 종료 메시지를 받으면 종료 코드를 반환한다.
    int MessageLoop();

    HWND GetWindowHandle() const;

    // 더블 버퍼링용 오프스크린 DC. WM_PAINT에서 여기에 전부 그린 뒤 화면에 한 번에 복사(BitBlt)해서 깜빡임을 없앤다.
    HDC GetBackBufferDC() const;

private:
    WindowManager() = default;
    ~WindowManager() = default;
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;

    HDC m_backBufferDC = nullptr;
    HBITMAP m_backBufferBitmap = nullptr;
};
