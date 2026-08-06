#include "pch.h"

#include <string>

#include "RenderManager.h"
#include "../util/Constants.h"
#include "../core/TimeManager.h"
#include "../managers/BlockManager.h"
#include "../managers/CameraManager.h"
#include "../managers/PhysicsManager.h"
#include "../objects/Block.h"

RenderManager& RenderManager::GetInstance()
{
	static RenderManager instance;
	return instance;
}

void RenderManager::DrawSpriteRotated(HDC hdc, const SpriteInfo& sprite, Vector2 center, Vector2 size, float angle, int frameIndex)
{
	Gdiplus::Graphics graphics(hdc);

	//픽셀이 흐리게 보이는 것을 해결하기 위해 추가한 코드
	graphics.SetInterpolationMode(Gdiplus::InterpolationMode::InterpolationModeNearestNeighbor);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);


	/*constexpr int FRAME_COUNT = 11;
	float frameWidth = static_cast<float>(sprite.bitmap->GetWidth()) / FRAME_COUNT;
	float frameHeight = static_cast<float>(sprite.bitmap->GetHeight());
	float srcX = frameIndex * frameWidth;*/

	float frameWidth = static_cast<float>(sprite.bitmap->GetWidth());
	float frameHeight = static_cast<float>(sprite.bitmap->GetHeight());
	float srcX = 0.0f;

	// center를 회전 피벗으로 삼아 그 자리에서 회전시킨 뒤, 크기의 절반만큼 되돌아가 그린다
	// (= 항상 center를 중심으로 하는 size x size 정사각형이 angle만큼 회전된 모습)
	graphics.TranslateTransform(center.x, center.y);
	graphics.RotateTransform(angle);
	graphics.TranslateTransform(-size.x / 2.0f, -size.y / 2.0f);

	graphics.DrawImage(
		sprite.bitmap.get(),
		Gdiplus::RectF(0.0f, 0.0f, size.x, size.y),
		srcX, 0.0f, frameWidth, frameHeight,
		Gdiplus::UnitPixel);
}

void RenderManager::Render(HDC hdc)
{
	HPEN gridPen = CreatePen(PS_SOLID, 1, Constants::GRID_LINE_COLOR);
	HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, gridPen));


	// 세로선 그리기: x=0(왼쪽 끝)부터 GRID_WIDTH(오른쪽 끝)까지.
	// 칸이 10개면 칸 사이 경계선은 11개라서 <=를 씀
	for (int x = 0; x <= Constants::GRID_WIDTH_SUBCELLS; ++x)
	{
		int px = static_cast<int>(x * Constants::SUBCELL_SIZE);

		// GDI는 "펜을 옮기고(MoveToEx) -> 그 자리에서 선을 긋는다(LineTo)" 방식으로 동작함
		MoveToEx(hdc, px, 0, nullptr);
		LineTo(hdc, px, Constants::WINDOW_HEIGHT);
	}

	// 가로선 그리기: 위와 동일한 방식으로 y=0부터 GRID_HEIGHT_SUBCELLS까지
	for (int y = 0; y <= Constants::GRID_HEIGHT_SUBCELLS; ++y)
	{
		int py = static_cast<int>(y * Constants::SUBCELL_SIZE);
		MoveToEx(hdc, 0, py, nullptr);
		LineTo(hdc, Constants::WINDOW_WIDTH, py);
	}

	SelectObject(hdc, oldPen);
	DeleteObject(gridPen);
}

void RenderManager::DrawFps(HDC hdc)
{
	int fps = static_cast<int>(TimeManager::GetInstance().GetFPS());
	std::string fpsText = "FPS: " + std::to_string(fps);

	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, Constants::FPS_TEXT_COLOR);
	TextOutA(hdc, Constants::FPS_TEXT_MARGIN, Constants::FPS_TEXT_MARGIN, fpsText.c_str(), static_cast<int>(fpsText.length()));
}

void RenderManager::DrawBlockColliders(HDC hdc)
{
	HPEN colliderPen = CreatePen(PS_SOLID, 1, Constants::COLLIDER_DEBUG_COLOR);
	HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, colliderPen));
	HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));

	for (Block* block : BlockManager::GetInstance().GetAllBlocks())
	{
		for (int cellIndex = 0; cellIndex < block->GetCellCount(); ++cellIndex)
		{
			// GetCellRotatedCorners가 실제 충돌 판정(SAT)에 쓰는 바로 그 사각형이라, 이걸 그대로 그리면
			// "진짜 콜라이더"가 어디에 있는지 보인다 (스프라이트 렌더링용 회전과는 별개 계산이라 어긋날 수 있음)
			Vector2 corners[4];
			block->GetCellRotatedCorners(cellIndex, corners);

			POINT screenPoints[5];
			for (int i = 0; i < 4; ++i)
			{
				Vector2 screenPos = CameraManager::GetInstance().WorldToScreen(corners[i]);
				screenPoints[i] = { static_cast<LONG>(screenPos.x), static_cast<LONG>(screenPos.y) };
			}
			screenPoints[4] = screenPoints[0]; // 사각형을 닫기 위해 첫 점으로 되돌아감

			Polyline(hdc, screenPoints, 5);
		}
	}

	SelectObject(hdc, oldBrush);
	SelectObject(hdc, oldPen);
	DeleteObject(colliderPen);
}

void RenderManager::DrawSupportDebug(HDC hdc)
{
	for (Block* block : BlockManager::GetInstance().GetAllBlocks())
	{
		if (block->GetPhysicsState() == PhysicsState::Airborne)
		{
			continue;
		}

		float minX = 0.0f, maxX = 0.0f, combinedComX = 0.0f;
		bool hasSupport = PhysicsManager::GetInstance().ComputeSupportDebugInfo(block, minX, maxX, combinedComX);
		if (!hasSupport)
		{
			continue;
		}

		// ResolveBalance와 같은 IMBALANCE_DEADZONE 여유를 적용해서, 실제로 넘어지지 않을 미세한
		// 오차까지 빨간색으로 표시해 헷갈리는 일이 없게 한다
		bool balanced = combinedComX >= minX - Constants::IMBALANCE_DEADZONE && combinedComX <= maxX + Constants::IMBALANCE_DEADZONE;
		COLORREF color = balanced ? RGB(0, 200, 80) : RGB(255, 0, 0);

		HPEN pen = CreatePen(PS_SOLID, 3, color);
		HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));

		// 블럭 위쪽 8px 지점에 지지 범위(가로선)를 그린다
		AABB bounds = block->GetWorldBounds();
		float lineY = bounds.min.y - 8.0f;

		Vector2 minScreen = CameraManager::GetInstance().WorldToScreen({ minX, lineY });
		Vector2 maxScreen = CameraManager::GetInstance().WorldToScreen({ maxX, lineY });
		MoveToEx(hdc, static_cast<int>(minScreen.x), static_cast<int>(minScreen.y), nullptr);
		LineTo(hdc, static_cast<int>(maxScreen.x), static_cast<int>(maxScreen.y));

		// 결합 무게중심 위치에 세로 틱을 그린다
		Vector2 comScreen = CameraManager::GetInstance().WorldToScreen({ combinedComX, lineY });
		MoveToEx(hdc, static_cast<int>(comScreen.x), static_cast<int>(comScreen.y) - 7, nullptr);
		LineTo(hdc, static_cast<int>(comScreen.x), static_cast<int>(comScreen.y) + 7);

		SelectObject(hdc, oldPen);
		DeleteObject(pen);
	}
}
