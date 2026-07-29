#include "pch.h"

#include "PlayScene.h"

void PlayScene::Enter()
{
    // TODO: 그리드/카메라 등 플레이 상태 초기화 직접 구현
}

void PlayScene::Exit()
{
    // TODO: 플레이 상태 정리 로직 직접 구현
}

void PlayScene::Update(float deltaTime)
{
    // TODO: BlockManager::UpdateFalling/MoveCurrentBlock(공중 블럭 그리드 낙하/이동),
    // PhysicsManager::Update(바닥 블럭 물리), CollisionManager, CameraManager, UIManager 갱신 호출 직접 구현
    (void)deltaTime;
}

void PlayScene::Render()
{
    // TODO: RenderManager/UIManager 렌더링 호출 직접 구현
}
