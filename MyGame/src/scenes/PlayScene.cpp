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
#include "../core/SceneManager.h"
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
}

void PlayScene::Update(float deltaTime)
{
    // TODO: CollisionManager, UIManager 갱신 호출 직접 구현
    BlockManager::GetInstance().UpdateFalling(deltaTime);
    BlockManager::GetInstance().UpdateMovementInput(deltaTime);
    BlockManager::GetInstance().UpdateRotationInput();
    PhysicsManager::GetInstance().Update(deltaTime);
    BlockManager::GetInstance().CheckDeathZone();

    if (BlockManager::GetInstance().IsGameOver())
    {
        // [중요] ChangeScene은 지금 이 PlayScene 인스턴스를 그 자리에서 파괴한다(SceneManager가
        // unique_ptr을 재할당하면서). 그러니 이 줄 다음엔 this(멤버 변수 포함)를 절대 건드리면 안 되고,
        // 바로 return해서 이 함수를 끝내야 한다.
        SceneManager::GetInstance().ChangeScene(SceneType::GameOver);
        return;
    }

    // 물리가 이번 프레임 블럭 위치를 확정한 뒤에 카메라가 그걸 보고 반응해야 한다
    CameraManager::GetInstance().UpdateTarget(BlockManager::GetInstance().GetAllBlocks());
    CameraManager::GetInstance().Update(deltaTime);

    if (InputManager::GetInstance().IsKeyPressed(VK_F1))
    {
        m_showColliders = !m_showColliders;
    }
	float currentHeightMeters = BlockManager::GetInstance().GetTallestHeightMeters();
    if (currentHeightMeters > m_bestHeightMeters)
    {
        m_bestHeightMeters = currentHeightMeters;
    }
}

void PlayScene::RenderBackground(ID2D1RenderTarget* renderTarget)
{
    //배경화면 그리기
    const SpriteInfo& backgroundSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/background.png");
    D2D1_SIZE_F backgroundSize = backgroundSprite.bitmap ? backgroundSprite.bitmap->GetSize() : D2D1::SizeF(0.0f, 0.0f);
    float backgroundHeight = backgroundSize.height;
    float backgroundWidth = backgroundSize.width;
    Vector2 backgroundPosition = { 0.0f, Constants::WINDOW_HEIGHT - backgroundHeight };
    Vector2 backgroundScreenPos = CameraManager::GetInstance().WorldToScreen(backgroundPosition);
    Vector2 backgroundCenter = backgroundScreenPos + Vector2{ backgroundWidth / 2.0f, backgroundHeight / 2.0f };
    RenderManager::GetInstance().DrawSpriteRotated(renderTarget, backgroundSprite, backgroundCenter, { backgroundWidth,backgroundHeight }, 0.0f, 0);

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

            RenderManager::GetInstance().DrawSpriteRotated(renderTarget, floorSprite, floorCenter, { Constants::TILE_SIZE, Constants::TILE_SIZE }, 0.0f, 0);
        }
    }
}

void PlayScene::RenderDeathZones(ID2D1RenderTarget* renderTarget)
{
    // 실제 판정(BlockManager::IsPointInDeathZone)과 정확히 같은 기준선(카메라를 따라 움직이는
    // GetDeathZoneTopY())을 써야 그림과 판정이 어긋나지 않는다. 화면엔 그 아래로 눈에 띌 만큼
    // (DEATH_ZONE_VISUAL_HEIGHT)만 그린다.
    float zoneTop = BlockManager::GetInstance().GetDeathZoneTopY();
    float zoneHeight = Constants::DEATH_ZONE_VISUAL_HEIGHT;

    float leftWidth = Constants::FLOOR_LEFT_X;
    Vector2 leftWorldCenter = { leftWidth / 2.0f, zoneTop + zoneHeight / 2.0f };
    Vector2 leftScreenCenter = CameraManager::GetInstance().WorldToScreen(leftWorldCenter);
    RenderManager::GetInstance().FillRect(renderTarget, leftScreenCenter, { leftWidth, zoneHeight }, Constants::DEATH_ZONE_COLOR, Constants::DEATH_ZONE_OPACITY);

    float rightWidth = static_cast<float>(Constants::WINDOW_WIDTH) - Constants::FLOOR_RIGHT_X;
    Vector2 rightWorldCenter = { Constants::FLOOR_RIGHT_X + rightWidth / 2.0f, zoneTop + zoneHeight / 2.0f };
    Vector2 rightScreenCenter = CameraManager::GetInstance().WorldToScreen(rightWorldCenter);
    RenderManager::GetInstance().FillRect(renderTarget, rightScreenCenter, { rightWidth, zoneHeight }, Constants::DEATH_ZONE_COLOR, Constants::DEATH_ZONE_OPACITY);
}

void PlayScene::Render(ID2D1RenderTarget* renderTarget)
{
    RenderBackground(renderTarget);
    RenderDeathZones(renderTarget);

    // 낙하 중이든 착지했든 전부 매 프레임 바뀔 수 있는 부분이라 매번 새로 그린다
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        const SpriteInfo& blockSprite = ResourceManager::GetInstance().GetSpriteInfo(block->GetSpriteId());
        for (int cellIndex = 0; cellIndex < block->GetCellCount(); ++cellIndex)
        {
            Vector2 cellScreenCenter = CameraManager::GetInstance().WorldToScreen(block->GetCellCenterRotated(cellIndex));
            RenderManager::GetInstance().DrawSpriteRotated(renderTarget, blockSprite, cellScreenCenter, { Constants::TILE_SIZE, Constants::TILE_SIZE }, block->GetAngle(), 0);
        }
    }

    RenderManager::GetInstance().DrawHeightRecord(renderTarget, m_bestHeightMeters);

    if (m_showColliders)
    {
        RenderManager::GetInstance().DrawBlockColliders(renderTarget);
        RenderManager::GetInstance().DrawSupportDebug(renderTarget);
        RenderManager::GetInstance().DrawPhysicsDebugText(renderTarget);
        RenderManager::GetInstance().DrawCenterOfMass(renderTarget);
    }
}
