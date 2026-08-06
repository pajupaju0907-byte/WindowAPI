#include "pch.h"

#include "PlayScene.h"
#include "../managers/ResourceManager.h"
#include "../managers/RenderManager.h"
#include "../managers/CameraManager.h"
#include "../managers/BlockManager.h"
#include "../objects/Block.h"
#include "../util/Constants.h"
#include "../util/Types.h"
#include "../managers/PhysicsManager.h"
#include "../core/InputManager.h"
void PlayScene::Enter()
{
    // TODO: 카메라 등 나머지 플레이 상태 초기화 직접 구현

    ResourceManager::GetInstance().LoadSprite("assets/block1.png");
    ResourceManager::GetInstance().LoadSprite("assets/block2.png");
    ResourceManager::GetInstance().LoadSprite("assets/block3.png");
    ResourceManager::GetInstance().LoadSprite("assets/block4.png");
    ResourceManager::GetInstance().LoadSprite("assets/block5.png");
    ResourceManager::GetInstance().LoadSprite("assets/block6.png");
    ResourceManager::GetInstance().LoadSprite("assets/block7.png");
    ResourceManager::GetInstance().LoadSprite("assets/background.png");
    ResourceManager::GetInstance().LoadSprite("assets/topleft.png");
    ResourceManager::GetInstance().LoadSprite("assets/topcenter.png");
    ResourceManager::GetInstance().LoadSprite("assets/topright.png");
    ResourceManager::GetInstance().LoadSprite("assets/bottomleft.png");
    ResourceManager::GetInstance().LoadSprite("assets/bottomcenter.png");
    ResourceManager::GetInstance().LoadSprite("assets/bottomright.png");

    // 낙하 테스트용 스폰 (임시 - 실제 스폰 규칙/다음 블럭 큐 등은 별도로 직접 설계할 것)
    BlockManager::GetInstance().SpawnBlock(BlockType::Tetromino);
}


void PlayScene::Exit()
{
    // TODO: 플레이 상태 정리 로직 직접 구현

    // RenderStaticLayer 캐시용으로 만들어둔 GDI 리소스 해제
    if (m_staticLayerDC != nullptr)
    {
        DeleteDC(m_staticLayerDC);
        DeleteObject(m_staticLayerBitmap);
        m_staticLayerDC = nullptr;
        m_staticLayerBitmap = nullptr;
    }
}

void PlayScene::Update(float deltaTime)
{
    // TODO: PhysicsManager::Update(바닥 블럭 물리), CollisionManager, CameraManager, UIManager 갱신 호출 직접 구현
    BlockManager::GetInstance().UpdateFalling(deltaTime);
    BlockManager::GetInstance().UpdateMovementInput(deltaTime);
    BlockManager::GetInstance().UpdateRotationInput();
    PhysicsManager::GetInstance().Update(deltaTime);

    if (InputManager::GetInstance().IsKeyPressed(VK_F1))
    {
        m_showColliders = !m_showColliders;
    }
}

void PlayScene::RenderStaticLayer(HDC hdc)
{
    //배경화면 그리기
    const SpriteInfo& backgroundSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/background.png");
    float backgroundHeight = static_cast<float>(backgroundSprite.bitmap->GetHeight());
    float backgroundWidth = static_cast<float>(backgroundSprite.bitmap->GetWidth());
    Vector2 backgroundPosition = { 0.0f, Constants::WINDOW_HEIGHT - backgroundHeight };
    Vector2 backgroundScreenPos = CameraManager::GetInstance().WorldToScreen(backgroundPosition);
    Vector2 backgroundCenter = backgroundScreenPos + Vector2{ backgroundWidth / 2.0f, backgroundHeight / 2.0f };
    RenderManager::GetInstance().DrawSpriteRotated(hdc, backgroundSprite, backgroundCenter, { backgroundWidth,backgroundHeight }, 0.0f, 0);

    for (int row = 0; row < Constants::FLOOR_HEIGHT_TILES; ++row)
    {
        for (int col = 0; col < Constants::FLOOR_WIDTH_TILES; ++col)
        {

            std::string spriteId;

            if (row == 0)
            {
                // 잔디 세트
                if (col == 0)
                {
                    spriteId = "assets/topleft.png";
                }
                else if (col == Constants::FLOOR_WIDTH_TILES - 1)
                {
                    spriteId = "assets/topright.png";
                }
                else
                {
                    spriteId = "assets/topcenter.png";
                }
            }
            else
            {
                // 흙 세트
                if (col == 0)
                {
                    spriteId = "assets/bottomleft.png";
                }
                else if (col == Constants::FLOOR_WIDTH_TILES - 1)
                {
                    spriteId = "assets/bottomright.png";
                }
                else
                {
                    spriteId = "assets/bottomcenter.png";
                }
            }
            const SpriteInfo& floorSprite = ResourceManager::GetInstance().GetSpriteInfo(spriteId);
            float floorLeftMargin = (Constants::GRID_WIDTH - Constants::FLOOR_WIDTH_TILES) / 2.0f * Constants::TILE_SIZE;
            Vector2 floorWorldPos = { floorLeftMargin + col * Constants::TILE_SIZE, Constants::WINDOW_HEIGHT - (Constants::FLOOR_HEIGHT_TILES - row) * Constants::TILE_SIZE };
            Vector2 floorScreenPos = CameraManager::GetInstance().WorldToScreen(floorWorldPos);
            Vector2 floorCenter = floorScreenPos + Vector2{ Constants::TILE_SIZE / 2.0f, Constants::TILE_SIZE / 2.0f };

            RenderManager::GetInstance().DrawSpriteRotated(hdc, floorSprite, floorCenter, { Constants::TILE_SIZE, Constants::TILE_SIZE }, 0.0f, 0);
        }
    }
}

void PlayScene::Render(HDC hdc)
{
    // 정적 레이어(배경+바닥)는 최초 1회만 그려서 캐시해두고, 이후엔 복사만 한다
    if (m_staticLayerDC == nullptr)
    {
        m_staticLayerDC = CreateCompatibleDC(hdc);
        m_staticLayerBitmap = CreateCompatibleBitmap(hdc, Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT);
        SelectObject(m_staticLayerDC, m_staticLayerBitmap);
    }

    if (m_staticLayerDirty)
    {
        RenderStaticLayer(m_staticLayerDC);
        m_staticLayerDirty = false;
    }

    BitBlt(hdc, 0, 0, Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT, m_staticLayerDC, 0, 0, SRCCOPY);

  
    // 낙하 중이든 착지했든 전부 매 프레임 바뀔 수 있는 부분이라 정적 레이어에 넣지 않고 여기서 매번 새로 그린다
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        const SpriteInfo& blockSprite = ResourceManager::GetInstance().GetSpriteInfo(block->GetSpriteId());
        for (int cellIndex = 0; cellIndex < block->GetCellCount(); ++cellIndex)
        {
            Vector2 cellScreenCenter = CameraManager::GetInstance().WorldToScreen(block->GetCellCenterRotated(cellIndex));
            RenderManager::GetInstance().DrawSpriteRotated(hdc, blockSprite, cellScreenCenter, { Constants::TILE_SIZE, Constants::TILE_SIZE }, block->GetAngle(), 0);
        }
    }

    if (m_showColliders)
    {
        RenderManager::GetInstance().DrawBlockColliders(hdc);
        RenderManager::GetInstance().DrawSupportDebug(hdc);
    }
}