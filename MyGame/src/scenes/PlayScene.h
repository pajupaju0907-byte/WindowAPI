#pragma once

#include "../core/Scene.h"

// 실제 게임 플레이가 진행되는 씬.
// PhysicsManager/CollisionManager/BlockManager/CameraManager/UIManager를 사용한다.
class PlayScene : public Scene
{
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render(ID2D1RenderTarget* renderTarget) override;

private:
    // 배경/바닥을 그린다. Direct2D는 하드웨어 가속이라 GDI+ 때와 달리 매 프레임 새로 그려도 충분히 빨라서,
    // 카메라가 움직일 때마다 무효화해야 했던 GDI 오프스크린 캐시(BitBlt 이중 버퍼링)를 걷어내고
    // 블럭처럼 매 프레임 직접 그리는 방식으로 단순화했다.
    void RenderBackground(ID2D1RenderTarget* renderTarget);

    // 발판 옆(밟을 땅이 없는) 데스존을 반투명 빨간 사각형으로 표시한다. F1 디버그와 무관하게 항상 그려진다.
    void RenderDeathZones(ID2D1RenderTarget* renderTarget);

    // Block.png(색깔별 정사각형이 가로로 이어진 시트)에서 slotIndex번째 칸만 잘라 쓸 소스 사각형(원본 픽셀 좌표)
    D2D1_RECT_F GetBlockColorSourceRect(int slotIndex) const;

    // F1로 토글하는 디버그 콜라이더(빨간 사각형) 표시 여부
    bool m_showColliders = false;
    // 지금까지 쌓은 탑의 최고 높이(m). 탑이 무너져서 현재 높이가 줄어도 이 값은 안 줄어든다
    float m_bestHeightMeters = 0.0f;
};
