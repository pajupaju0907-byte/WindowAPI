#include "RenderManager.h"

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

void RenderManager::Render()
{
    // TODO: BlockManager/CameraManager/ResourceManager를 읽어 화면을 그리는 로직 직접 구현
}
