#pragma once

#include "../util/Types.h"


// 화면에 실제로 그리는 역할만 담당하는 싱글톤.
// 다른 매니저의 상태를 읽기만 하며, 게임 로직을 갖지 않는다.
class RenderManager
{
public:
	static RenderManager& GetInstance();
	/// @brief 스프라이트를 지정한 중심점 기준으로 회전시켜 그린다.
	/// @param hdc 그릴 대상 디바이스 컨텍스트
	/// @param sprite 그릴 스프라이트 정보
	/// @param center 회전 피벗이자 스프라이트의 중심이 위치할 좌표 (좌상단이 아님 — 회전이 있는 블럭은
	///        반드시 무게중심 기준으로 이미 회전된 칸 중심(Block::GetCellCenterRotated)을 넘겨야 콜라이더와 일치한다)
	/// @param angle 회전 각도(도 단위)
	/// @param  size 그릴 가로/세로	크기
	void DrawSpriteRotated(HDC hdc,const SpriteInfo& sprite,Vector2 center,Vector2 size, float angle, int frameIndex);

	/// @brief 현재 프레임의 화면을 그린다 (지금은 격자선만).
	/// @param hdc WM_PAINT에서 얻은 디바이스 컨텍스트
	void Render(HDC hdc);

	/// @brief 좌상단에 현재 FPS를 텍스트로 표시한다 (디버그용). 다른 내용 위에 덮이지 않도록 항상 맨 마지막에 그려야 한다.
	/// @param hdc WM_PAINT에서 얻은 디바이스 컨텍스트
	void DrawFps(HDC hdc);

	/// @brief 모든 블럭의 칸(cell)마다 실제 충돌 판정에 쓰이는 회전된 사각형(콜라이더)을 빨간 테두리로 그린다 (디버그용).
	/// @param hdc WM_PAINT에서 얻은 디바이스 컨텍스트
	void DrawBlockColliders(HDC hdc);

	/// @brief 착지한 블럭마다 지지 범위(가로선)와 결합 무게중심(세로 틱)을 그린다 (디버그용).
	/// 무게중심이 지지 범위 안이면 초록, 벗어났으면 빨강 — "왜 안 넘어지는지"를 코드 안 보고 눈으로 바로 확인용
	/// @param hdc WM_PAINT에서 얻은 디바이스 컨텍스트
	void DrawSupportDebug(HDC hdc);

private:
	RenderManager() = default;
	~RenderManager() = default;
	RenderManager(const RenderManager&) = delete;
	RenderManager& operator=(const RenderManager&) = delete;
};
