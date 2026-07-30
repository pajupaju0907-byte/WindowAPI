#include "pch.h"

#include "WindowManager.h"
#include "../util/Constants.h"
#include "../managers/RenderManager.h"
#include "SceneManager.h"
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

			// 실제 그리기는 RenderManager에 위임 (여기서 격자선이 그려짐)
			RenderManager::GetInstance().Render(hdc);
			SceneManager::GetInstance().Render(hdc);

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
	return true;
}

int WindowManager::MessageLoop()
{
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return static_cast<int>(msg.wParam);
}

HWND WindowManager::GetWindowHandle() const
{
	return m_hWnd;
}
