#include "pch.h"

#include "PlayScene.h"
#include "../managers/ResourceManager.h"
#include "../managers/RenderManager.h"
#include "../managers/CameraManager.h"
#include "../managers/BlockManager.h"
#include "../managers/SoundManager.h"
#include "../objects/Block.h"
#include "../util/Constants.h"
#include "../util/Types.h"
#include "../managers/PhysicsManager.h"
#include "../core/InputManager.h"
#include "../core/SceneManager.h"
#include <cmath>
void PlayScene::Enter()
{
	BlockManager::GetInstance().Reset();
	CameraManager::GetInstance().ResetCamera();

    ResourceManager::GetInstance().LoadSprite("assets/BlockT.png");
    ResourceManager::GetInstance().LoadSprite("assets/background.png");
    ResourceManager::GetInstance().LoadSprite("assets/topleft.png");
    ResourceManager::GetInstance().LoadSprite("assets/topcenter.png");
    ResourceManager::GetInstance().LoadSprite("assets/topright.png");
    ResourceManager::GetInstance().LoadSprite("assets/bottomleft.png");
    ResourceManager::GetInstance().LoadSprite("assets/bottomcenter.png");
    ResourceManager::GetInstance().LoadSprite("assets/bottomright.png");
    ResourceManager::GetInstance().LoadSprite("assets/Next.png");

    SoundManager::GetInstance().PlayBgm("assets/Sound/IlliyardMoor.mp3");

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

void PlayScene::RenderDropGuide(ID2D1RenderTarget* renderTarget)
{
    Block* fallingBlock = BlockManager::GetInstance().GetCurrentFallingBlock();
    if (fallingBlock == nullptr)
    {
        return;
    }

    // 같은 열(같은 x)에 칸이 여러 개(예: 세로 2칸) 있으면 그중 가장 아래 칸만 기준으로 삼아야
    // 열마다 사각형이 겹쳐서 그만큼 더 진하게 보이는 걸 막을 수 있다
    constexpr int MAX_COLUMNS = 4;
    float columnX[MAX_COLUMNS];
    float columnBottomY[MAX_COLUMNS];
    int columnCount = 0;

    for (int cellIndex = 0; cellIndex < fallingBlock->GetCellCount(); ++cellIndex)
    {
        Vector2 cellPos = fallingBlock->GetCellRenderPosition(cellIndex);
        float cellBottomY = cellPos.y + Constants::TILE_SIZE;

        int existingColumn = -1;
        for (int c = 0; c < columnCount; ++c)
        {
            if (fabsf(columnX[c] - cellPos.x) < 0.5f)
            {
                existingColumn = c;
                break;
            }
        }

        if (existingColumn == -1)
        {
            columnX[columnCount] = cellPos.x;
            columnBottomY[columnCount] = cellBottomY;
            ++columnCount;
        }
        else if (cellBottomY > columnBottomY[existingColumn])
        {
            columnBottomY[existingColumn] = cellBottomY;
        }
    }

    // 실제 착지 지점이 아니라 항상 끝까지 끊기지 않고 이어서 그린다. 중간에 이미 쌓인 블럭이 있어도
    // 이 가이드보다 나중에(위에) 그려지므로 그 구간만 자연스럽게 가려질 뿐, 가이드 자체가 착지
    // 지점에서 뚝 끊긴 것처럼 보이진 않는다.
    for (int c = 0; c < columnCount; ++c)
    {
        // 발판(FLOOR_LEFT_X~FLOOR_RIGHT_X) 위 열은 바닥 윗면에서 끊고, 그 옆 절벽(발판 없는 열)은
        // 바닥이 없으니 대신 지금 보이는 화면 맨 아래(BlockManager::GetDeathZoneTopY, 카메라를 따라
        // 움직임)까지 그린다 — 안 그러면 발판 옆에서만 유독 짧게 뚝 끊겨 보인다.
        bool overlapsFloor = columnX[c] + Constants::TILE_SIZE > Constants::FLOOR_LEFT_X && columnX[c] < Constants::FLOOR_RIGHT_X;
        float guideBottomY = overlapsFloor ? Constants::FLOOR_TOP_Y : BlockManager::GetInstance().GetDeathZoneTopY();
        if (guideBottomY <= columnBottomY[c])
        {
            continue;
        }

        Vector2 worldCenter = { columnX[c] + Constants::TILE_SIZE / 2.0f, (columnBottomY[c] + guideBottomY) / 2.0f };
        Vector2 screenCenter = CameraManager::GetInstance().WorldToScreen(worldCenter);
        RenderManager::GetInstance().FillRect(renderTarget, screenCenter, { Constants::TILE_SIZE, guideBottomY - columnBottomY[c] }, Constants::DROP_GUIDE_COLOR, Constants::DROP_GUIDE_OPACITY);
    }
}

void PlayScene::RenderNextBlockPreview(ID2D1RenderTarget* renderTarget)
{
    Vector2 panelCenter = { Constants::NEXT_PANEL_CENTER_X, Constants::NEXT_PANEL_CENTER_Y };

    const SpriteInfo& panelSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Next.png");
    if (panelSprite.bitmap)
    {
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, panelSprite, panelCenter, { Constants::NEXT_PANEL_TARGET_WIDTH, Constants::NEXT_PANEL_TARGET_WIDTH }, 0.0f, 0);
    }

    const Block* nextBlock = BlockManager::GetInstance().GetNextBlockPreview();
    if (nextBlock == nullptr)
    {
        return;
    }

    // nextBlock은 그리드 위치를 지정한 적이 없어 (0,0) 기준 로컬 좌표로 칸들이 나온다. 그 모양
    // 자체의 가로세로 중심(GetWorldBounds)을 구해서, 그 중심이 판넬 중앙(+세로 오프셋)에 오도록 맞춰 그린다
    AABB shapeBounds = nextBlock->GetWorldBounds();
    Vector2 shapeCenterLocal = { (shapeBounds.min.x + shapeBounds.max.x) / 2.0f, (shapeBounds.min.y + shapeBounds.max.y) / 2.0f };
    float scale = Constants::NEXT_PANEL_PREVIEW_TILE_SIZE / Constants::TILE_SIZE;
    Vector2 previewCenter = panelCenter + Vector2{ 0.0f, Constants::NEXT_PANEL_PREVIEW_OFFSET_Y };

    const SpriteInfo& blockSprite = ResourceManager::GetInstance().GetSpriteInfo(nextBlock->GetSpriteId());
    D2D1_RECT_F colorSourceRect = GetBlockColorSourceRect(nextBlock->GetColorSlotIndex());

    for (int cellIndex = 0; cellIndex < nextBlock->GetCellCount(); ++cellIndex)
    {
        Vector2 cellLocalCenter = nextBlock->GetCellRenderPosition(cellIndex) + Vector2{ Constants::TILE_SIZE / 2.0f, Constants::TILE_SIZE / 2.0f };
        Vector2 drawCenter = previewCenter + (cellLocalCenter - shapeCenterLocal) * scale;

        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, blockSprite, drawCenter, { Constants::NEXT_PANEL_PREVIEW_TILE_SIZE, Constants::NEXT_PANEL_PREVIEW_TILE_SIZE }, 0.0f, 0, 1.0f, &colorSourceRect);
    }
}

D2D1_RECT_F PlayScene::GetBlockColorSourceRect(int slotIndex) const
{
    float left = Constants::BLOCK_COLOR_SHEET_CONTENT_LEFT + Constants::BLOCK_COLOR_SHEET_SLOT_WIDTH * static_cast<float>(slotIndex);
    float top = Constants::BLOCK_COLOR_SHEET_CONTENT_TOP;
    return D2D1::RectF(left, top, left + Constants::BLOCK_COLOR_SHEET_SLOT_WIDTH, top + Constants::BLOCK_COLOR_SHEET_SLOT_HEIGHT);
}

void PlayScene::Render(ID2D1RenderTarget* renderTarget)
{
    RenderBackground(renderTarget);
    RenderDeathZones(renderTarget);
    RenderDropGuide(renderTarget);

    // 낙하 중이든 착지했든 전부 매 프레임 바뀔 수 있는 부분이라 매번 새로 그린다
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        const SpriteInfo& blockSprite = ResourceManager::GetInstance().GetSpriteInfo(block->GetSpriteId());
        D2D1_RECT_F colorSourceRect = GetBlockColorSourceRect(block->GetColorSlotIndex());
        for (int cellIndex = 0; cellIndex < block->GetCellCount(); ++cellIndex)
        {
            Vector2 cellScreenCenter = CameraManager::GetInstance().WorldToScreen(block->GetCellCenterRotated(cellIndex));
            RenderManager::GetInstance().DrawSpriteRotated(renderTarget, blockSprite, cellScreenCenter, { Constants::TILE_SIZE, Constants::TILE_SIZE }, block->GetAngle(), 0, 1.0f, &colorSourceRect);
        }
    }

    RenderManager::GetInstance().DrawHeightRecord(renderTarget, m_bestHeightMeters);
    RenderNextBlockPreview(renderTarget);

    if (m_showColliders)
    {
        RenderManager::GetInstance().DrawBlockColliders(renderTarget);
        RenderManager::GetInstance().DrawSupportDebug(renderTarget);
        RenderManager::GetInstance().DrawPhysicsDebugText(renderTarget);
        RenderManager::GetInstance().DrawCenterOfMass(renderTarget);
    }
}
