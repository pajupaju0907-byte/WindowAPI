#pragma once

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

// Win32 윈도우 생성과 메시지 루프, Direct2D/DirectWrite/WIC 디바이스 리소스를 담당하는 싱글톤
class WindowManager
{
public:
    static WindowManager& GetInstance();

    // 윈도우 클래스 등록/생성 및 Direct2D 렌더타겟, DirectWrite/WIC 팩토리 생성
    bool Init(HINSTANCE hInstance, int nCmdShow);

    // 기본 메시지 루프. 종료 메시지를 받으면 종료 코드를 반환한다.
    int MessageLoop();

    HWND GetWindowHandle() const;

    // 모든 그리기가 이 렌더타겟에 이루어진다. BeginDraw()~EndDraw() 사이에서만 유효하다.
    ID2D1HwndRenderTarget* GetRenderTarget() const;
    // 텍스트 그리기(DrawFps 등)에 필요한 DirectWrite 팩토리
    IDWriteFactory* GetWriteFactory() const;
    // 스프라이트 PNG 디코딩에 필요한 WIC 팩토리
    IWICImagingFactory* GetWicFactory() const;

    // 렌더타겟이 유실됐을 때(EndDraw가 D2DERR_RECREATE_TARGET을 반환) 같은 크기로 다시 만든다
    void RecreateRenderTarget();

private:
    WindowManager() = default;
    ~WindowManager() = default;
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    bool CreateRenderTarget();

    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_writeFactory;
    Microsoft::WRL::ComPtr<IWICImagingFactory> m_wicFactory;
};
