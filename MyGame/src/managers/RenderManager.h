#pragma once

#include "../util/Types.h"


// 화면에 실제로 그리는 역할만 담당하는 싱글톤.
// 다른 매니저의 상태를 읽기만 하며, 게임 로직을 갖지 않는다.
class RenderManager
{
public:
	static RenderManager& GetInstance();
	/// @brief 스프라이트를 지정한 위치에 회전시켜 그린다.
	/// @param hdc 그릴 대상 디바이스 컨텍스트
	/// @param sprite 그릴 스프라이트 정보
	/// @param position 그릴 위치 (좌상단 기준)
	/// @param angle 회전 각도(도 단위)
	/// @param  size 그릴 가로/세로	크기
	void DrawSpriteRotated(HDC hdc,const SpriteInfo& sprite,Vector2 Position,Vector2 size, float angle, int frameIndex);

	/// @brief 현재 프레임의 화면을 그린다 (지금은 격자선만).
	/// @param hdc WM_PAINT에서 얻은 디바이스 컨텍스트
	void Render(HDC hdc);

	/// @brief 좌상단에 현재 FPS를 텍스트로 표시한다 (디버그용). 다른 내용 위에 덮이지 않도록 항상 맨 마지막에 그려야 한다.
	/// @param hdc WM_PAINT에서 얻은 디바이스 컨텍스트
	void DrawFps(HDC hdc);

private:
	RenderManager() = default;
	~RenderManager() = default;
	RenderManager(const RenderManager&) = delete;
	RenderManager& operator=(const RenderManager&) = delete;
};
