#include "pch.h"

#include "RenderManager.h"
#include "../util/Constants.h"

RenderManager& RenderManager::GetInstance()
{
	static RenderManager instance;
	return instance;
}

void RenderManager::DrawSpriteRotated(HDC hdc, const SpriteInfo& sprite, Vector2 Position, float angle, int frameIndex)
{
	Gdiplus::Graphics graphics(hdc);

	constexpr int FRAME_COUNT = 11;
	float frameWidth = static_cast<float>(sprite.bitmap->GetWidth()) / FRAME_COUNT;
	float frameHeight = static_cast<float>(sprite.bitmap->GetHeight());
	float srcX = frameIndex * frameWidth;

	float size = Constants::TILE_SIZE;

	graphics.TranslateTransform(Position.x + size / 2.0f, Position.y + size / 2.0f);
	graphics.RotateTransform(angle);
	graphics.TranslateTransform(-size / 2.0f, -size / 2.0f);

	graphics.DrawImage(
		sprite.bitmap.get(),
		Gdiplus::RectF(0.0f, 0.0f, size, size),
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
