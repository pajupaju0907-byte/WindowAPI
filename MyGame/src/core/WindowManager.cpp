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
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_PAINT:

		{
			// 창을 다시 그려야 할 때(예: 다른 창에 가려졌다가 돌아옴) 윈도우가 이 메시지를 보냄
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			// 화면 DC에 직접 그리면 깜빡이므로, 오프스크린 백버퍼에 전부 그린 뒤 한 번에 복사한다
			HDC backBufferDC = WindowManager::GetInstance().GetBackBufferDC();
			RenderManager::GetInstance().Render(backBufferDC);
			SceneManager::GetInstance().Render(backBufferDC);
			// FPS 표시는 다른 내용에 덮이지 않도록 맨 마지막에 그린다
			RenderManager::GetInstance().DrawFps(backBufferDC);
			BitBlt(hdc, 0, 0, Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT, backBufferDC, 0, 0, SRCCOPY);

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

	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);

	// 더블 버퍼링용 백버퍼를 창 크기에 맞춰 한 번만 생성해두고 매 프레임 재사용한다
	HDC windowDC = GetDC(m_hWnd);
	m_backBufferDC = CreateCompatibleDC(windowDC);
	m_backBufferBitmap = CreateCompatibleBitmap(windowDC, Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT);
	SelectObject(m_backBufferDC, m_backBufferBitmap);
	ReleaseDC(m_hWnd, windowDC);

	return true;
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
			if (remainingSeconds > 0.0)
			{
				Sleep(static_cast<DWORD>(remainingSeconds * 1000.0));
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

HDC WindowManager::GetBackBufferDC() const
{
	return m_backBufferDC;
}
