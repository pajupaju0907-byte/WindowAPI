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
    void Render(HDC hdc) override;

private:
    // 배경/바닥처럼 프레임마다 안 바뀌는 부분을 한 번만 그려두는 캐시 레이어.
    // 매 프레임 GDI+로 다시 그리는 대신 이걸 그대로 복사(BitBlt)만 해서 렌더링 비용을 줄인다.
    // 주의: 나중에 카메라가 실제로 움직이기 시작하면(CameraManager::Update 구현 시) 배경/바닥의
    // 화면 좌표도 매번 바뀌므로, 그때는 카메라가 움직일 때마다 m_staticLayerDirty를 다시 true로
    // 만들어줘야 한다 (지금은 카메라가 고정이라 처음 한 번만 그리면 계속 유효함).
    void RenderStaticLayer(HDC hdc);

    HDC m_staticLayerDC = nullptr;
    HBITMAP m_staticLayerBitmap = nullptr;
    bool m_staticLayerDirty = true;

    // F1로 토글하는 디버그 콜라이더(빨간 사각형) 표시 여부
    bool m_showColliders = false;
};
