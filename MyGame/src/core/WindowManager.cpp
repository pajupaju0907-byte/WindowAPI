#include "pch.h"

#include <chrono>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#include "WindowManager.h"
#include "../util/Constants.h"
#include "../managers/RenderManager.h"
#include "SceneManager.h"
#include "TimeManager.h"
#include "InputManager.h"
namespace
{
	const wchar_t* WINDOW_CLASS_NAME = L"WobbleBlocksWindowClass";

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CHAR:
			InputManager::GetInstance().OnChar(static_cast<wchar_t>(wParam));
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_PAINT:

		{
			// 창을 다시 그려야 할 때(예: 다른 창에 가려졌다가 돌아옴) 윈도우가 이 메시지를 보냄
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);

			// Direct2D는 GDI처럼 별도 백버퍼를 손으로 관리할 필요가 없다 — HwndRenderTarget이
			// BeginDraw()~EndDraw() 사이에 그린 내용을 내부적으로 처리하고 EndDraw()에서 화면에 반영한다.
			// [방어] 렌더타겟이 아직(또는 재생성 실패로) 없으면 null 포인터 호출로 콜백 안에서 죽는 대신 이번 프레임만 건너뛴다
			ID2D1HwndRenderTarget* renderTarget = WindowManager::GetInstance().GetRenderTarget();
			if (renderTarget == nullptr)
			{
				EndPaint(hWnd, &ps);
				return 0;
			}

			renderTarget->BeginDraw();
			renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

			RenderManager::GetInstance().Render(renderTarget);
			SceneManager::GetInstance().Render(renderTarget);
			// FPS 표시는 다른 내용에 덮이지 않도록 맨 마지막에 그린다
			RenderManager::GetInstance().DrawFps(renderTarget);

			HRESULT hr = renderTarget->EndDraw();
			if (hr == D2DERR_RECREATE_TARGET)
			{
				// 디바이스 유실(그래픽 드라이버 리셋 등) — 렌더타겟만 다시 만든다. 이미 로드된 스프라이트
				// 비트맵은 옛 렌더타겟에 종속돼 있어 같이 무효화되지만, 흔치 않은 상황이라 재로딩까지는 다루지 않는다.
				WindowManager::GetInstance().RecreateRenderTarget();
			}

			EndPaint(hWnd, &ps);
			return 0;
		}
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
}

WindowManager& WindowManager::GetInstance()
{
	static WindowManager instance;
	return instance;
}

bool WindowManager::Init(HINSTANCE hInstance, int nCmdShow)
{
	m_hInstance = hInstance;

	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	wcex.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClassExW(&wcex))
	{
		return false;
	}

	// CreateWindowW의 nWidth/nHeight는 테두리+타이틀바를 포함한 창 전체 크기이므로,
	// 클라이언트 영역(캔버스)이 정확히 Constants::WINDOW_WIDTH/HEIGHT가 되도록 보정한다.
	RECT windowRect = { 0, 0, Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	m_hWnd = CreateWindowW(WINDOW_CLASS_NAME, L"Wobble Blocks", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
		nullptr, nullptr, hInstance, nullptr);

	if (!m_hWnd)
	{
		return false;
	}

	// [중요] Direct2D/DirectWrite/WIC 리소스를 전부 준비하기 전에는 창을 보여주면 안 된다.
	// ShowWindow/UpdateWindow가 창을 보여주는 순간 WM_PAINT가 그 자리에서 바로 호출될 수 있는데,
	// 그때 렌더타겟이 아직 null이면 WndProc 안에서 그 null 포인터로 BeginDraw()를 호출하다가
	// 콜백 안에서 처리 안 된 예외로 즉시 크래시가 난다(GDI 핸들과 달리 COM 포인터는 null이면 호출 자체가 죽는다).
	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf())))
	{
		return false;
	}

	if (!CreateRenderTarget())
	{
		return false;
	}

	// 텍스트(FPS/물리 디버그) 그리기용 DirectWrite 팩토리. 렌더타겟과 달리 디바이스에 종속되지 않아 한 번만 만든다
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(m_writeFactory.GetAddressOf()))))
	{
		return false;
	}

	// 스프라이트 PNG 디코딩용 WIC 팩토리. 사용하려면 호출 스레드에 COM이 초기화돼 있어야 한다(MyGame.cpp에서 처리)
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(m_wicFactory.GetAddressOf()))))
	{
		return false;
	}

	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);

	return true;
}

bool WindowManager::CreateRenderTarget()
{
	m_renderTarget.Reset();

	RECT clientRect{};
	GetClientRect(m_hWnd, &clientRect);
	D2D1_SIZE_U size = D2D1::SizeU(
		static_cast<UINT32>(clientRect.right - clientRect.left),
		static_cast<UINT32>(clientRect.bottom - clientRect.top));

	// [중요] 기본값(D2D1_PRESENT_OPTIONS_NONE)은 EndDraw()가 모니터 수직동기화(vsync)를 기다렸다가 화면에
	// 낸다 — 이러면 TARGET_FPS를 아무리 올려도 모니터 주사율(보통 60Hz)을 못 넘는다. IMMEDIATELY를 주면
	// vsync를 안 기다리고 그리는 즉시 내보내서, MessageLoop의 Sleep 기반 프레임 제한이 실제로 상한이 된다.
	HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(m_hWnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
		m_renderTarget.GetAddressOf());

	return SUCCEEDED(hr);
}

void WindowManager::RecreateRenderTarget()
{
	CreateRenderTarget();
}

int WindowManager::MessageLoop()
{
	// Sleep()의 기본 타이머 정밀도(~15ms)로는 60fps(16.67ms) 단위를 못 맞춰서 오차가 크게 남.
	// 1ms 단위로 요청해 Sleep 정밀도를 올린다. 루프를 벗어날 때 반드시 짝을 맞춰 해제해야 한다.
	timeBeginPeriod(1);

	auto lastFrameTime = std::chrono::steady_clock::now();

	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// 목표 프레임 시간(Constants::TARGET_FRAME_SECONDS)이 아직 안 지났으면
			// 남은 시간만큼 재워서 60fps로 제한하고, CPU를 계속 100% 쓰지 않게 한다
			double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - lastFrameTime).count();
			double remainingSeconds = Constants::TARGET_FRAME_SECONDS - elapsedSeconds;
			if (remainingSeconds > Constants::FRAME_LIMITER_SPIN_MARGIN_SECONDS)
			{
				// [프레임 페이싱] 마지막 스핀 마진만큼은 남겨두고 잔다 — Sleep()은 그 구간까지 정확히
				// 재우려 하면 OS 스케줄러 지터로 목표 시간을 넘겨버리기 쉽다.
				double sleepSeconds = remainingSeconds - Constants::FRAME_LIMITER_SPIN_MARGIN_SECONDS;
				Sleep(static_cast<DWORD>(sleepSeconds * 1000.0));
				continue;
			}
			if (remainingSeconds > 0.0)
			{
				// [프레임 페이싱] 마지막 자투리 시간은 Sleep 없이 스핀(바쁜 대기)으로 채워서, 그
				// 시점의 실제 경과 시간이 목표를 최대한 정확히 맞추게 한다
				continue;
			}
			lastFrameTime = std::chrono::steady_clock::now();

			// 처리할 메시지가 없는 동안이 한 프레임: 갱신 후 다시 그리도록 요청
			TimeManager::GetInstance().Update();
			InputManager::GetInstance().Update();
			SceneManager::GetInstance().Update(TimeManager::GetInstance().GetDeltaTime());
			InvalidateRect(m_hWnd, nullptr, FALSE);
		}
	}

	timeEndPeriod(1);
	return static_cast<int>(msg.wParam);
}

HWND WindowManager::GetWindowHandle() const
{
	return m_hWnd;
}

ID2D1HwndRenderTarget* WindowManager::GetRenderTarget() const
{
	return m_renderTarget.Get();
}

IDWriteFactory* WindowManager::GetWriteFactory() const
{
	return m_writeFactory.Get();
}

IWICImagingFactory* WindowManager::GetWicFactory() const
{
	return m_wicFactory.Get();
}

void WindowManager::Shutdown()
{
	m_renderTarget.Reset();
	m_writeFactory.Reset();
	m_wicFactory.Reset();
	m_d2dFactory.Reset();
}
