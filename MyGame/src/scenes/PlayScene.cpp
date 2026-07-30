#include "pch.h"

#include "PlayScene.h"
#include "../managers/ResourceManager.h"
#include "../managers/RenderManager.h"

void PlayScene::Enter()
{
    // TODO: 그리드/카메라 등 플레이 상태 초기화 직접 구현
    ResourceManager::GetInstance().LoadSprite("assets/block.png");
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

void PlayScene::Render(HDC hdc)
{
    const SpriteInfo& blockSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/block.png");
    RenderManager::GetInstance().DrawSpriteRotated(hdc, blockSprite, { 0.0f, 0.0f }, 0.0f, 0);
}