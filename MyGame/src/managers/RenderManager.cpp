#include "pch.h"

#include "RenderManager.h"
#include "../util/Constants.h"

RenderManager& RenderManager::GetInstance()
{
    static RenderManager instance;
    return instance;
}

void RenderManager::DrawSpriteRotated(const SpriteInfo& sprite, float angle)
{
    // TODO: 회전된 스프라이트 실제 그리기 로직 직접 구현
    (void)sprite;
    (void)angle;
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
