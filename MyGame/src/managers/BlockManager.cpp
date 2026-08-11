#include "pch.h"

#include <cmath>
#include <cstdlib>

#include "BlockManager.h"
#include "../objects/Block.h"
#include "../objects/TetrominoBlock.h"
#include "../objects/GiantTetrominoBlock.h"
#include "../objects/HeavyBlock.h"
#include "../util/Constants.h"
#include "../core/InputManager.h"
#include "CameraManager.h"
BlockManager& BlockManager::GetInstance()
{
    static BlockManager instance;
    return instance;
}

BlockManager::~BlockManager() = default;

void BlockManager::SpawnBlock(BlockType type)
{
    std::unique_ptr<Block> spawnedBlock;
    switch (type)
    {
    case BlockType::Tetromino:
    {
        TetrominoShape randomShape = static_cast<TetrominoShape>(rand() % 7);
        spawnedBlock = std::make_unique<TetrominoBlock>(randomShape);
    }
    break;
    case BlockType::GiantTetromino:
        spawnedBlock = std::make_unique<GiantTetrominoBlock>();
        break;
    case BlockType::Heavy:
        spawnedBlock = std::make_unique<HeavyBlock>();
        break;
    }

    // 그리드 가로 중앙에서 스폰. 세로는 고정된 0번 줄이 아니라 카메라가 보고 있는 화면 맨 위 지점
    // (world y = 카메라 위치)에 맞춰 스폰한다 — 카메라가 위로 올라간 만큼 스폰 위치도 같이 올라가서,
    // 항상 화면 맨 위에서 블럭이 등장하는 것처럼 보이게 한다. 카메라가 안 움직인 초반엔 위치가 0이라
    // 기존과 동일하게 맨 위 줄에서 스폰된다.
    float cameraTopWorldY = CameraManager::GetInstance().GetPosition().y;
    int spawnGridY = static_cast<int>(std::round(cameraTopWorldY / Constants::SUBCELL_SIZE));
    spawnedBlock->SetGridPosition(Constants::GRID_WIDTH_SUBCELLS / 2, spawnGridY);

    m_currentFallingBlock = spawnedBlock.get();
    m_blocks.push_back(std::move(spawnedBlock));
    m_fallTimer = Constants::FALL_STEP_INTERVAL;
}

void BlockManager::UpdateFalling(float deltaTime)
{
    if (m_currentFallingBlock == nullptr) return;

    // [락 딜레이] 낙하 타이머(m_fallTimer)와는 별개로, 매 프레임 "지금 내려갈 수 있는 상태인지"를
    // 확인한다. 내려갈 수 있으면 딜레이를 취소하고, 못 내려가는 상태가 LOCK_DELAY_DURATION만큼
    // 이어지면 그때 착지를 확정한다 — 그래야 좁은 틈으로 옆으로 비켜 넣는 동안은 락이 안 걸린다.
    if (m_currentFallingBlock->CanStepDown())
    {
        m_lockDelayTimer = 0.0f;
    }
    else
    {
        m_lockDelayTimer += deltaTime;
        if (m_lockDelayTimer >= Constants::LOCK_DELAY_DURATION)
        {
            LockBlock(m_currentFallingBlock);
            m_currentFallingBlock = nullptr;
            m_lockDelayTimer = 0.0f;
            SpawnBlock(BlockType::Tetromino);
            return;
        }
    }

	float fallSpeedMultiplier = InputManager::GetInstance().IsKeyDown(VK_DOWN) ? Constants::SOFT_DROP_MULTIPLIER : 1.0f;
	m_fallTimer -= deltaTime * fallSpeedMultiplier;

    if (m_fallTimer <= 0.0f)
    {
		m_currentFallingBlock->StepDown(); // 실패(더 못 내려감)해도 락 판단은 위에서 이미 처리했으니 무시
		m_fallTimer += Constants::FALL_STEP_INTERVAL;
    }
}

void BlockManager::MoveCurrentBlock(int subCellDelta)
{
    if (m_currentFallingBlock == nullptr) return;

    m_currentFallingBlock->MoveHorizontal(subCellDelta);
}

void BlockManager::UpdateMovementInput(float deltaTime)
{
    InputManager& input = InputManager::GetInstance();

    int direction = 0;
    if (input.IsKeyDown(VK_LEFT)) direction = -1;
    else if (input.IsKeyDown(VK_RIGHT)) direction = 1;

    if (direction == 0)
    {
        return;
    }

    // 새로 누른 순간이면 타이머를 0으로 만들어서 이번 프레임에 바로 이동하게 한다
    if (input.IsKeyPressed(VK_LEFT) || input.IsKeyPressed(VK_RIGHT))
    {
        m_moveTimer = 0.0f;
    }

    m_moveTimer -= deltaTime;
    if (m_moveTimer <= 0.0f)
    {
        MoveCurrentBlock(direction * Constants::MOVE_STEP_SUBCELLS);
        m_moveTimer += Constants::MOVE_REPEAT_INTERVAL;
    }
}

void BlockManager::UpdateRotationInput()
{
    if (m_currentFallingBlock == nullptr) return;

    InputManager& input = InputManager::GetInstance();

    if (input.IsKeyPressed('Z'))
    {
        m_currentFallingBlock->Rotate(-1);
    }
    else if (input.IsKeyPressed('X'))
    {
        m_currentFallingBlock->Rotate(1);
    }
}

void BlockManager::LockBlock(Block* block)
{
    if (block == nullptr) return;

    block->Land();
}

bool BlockManager::IsGameOver() const
{
    return m_isGameOver;
}

float BlockManager::GetDeathZoneTopY() const
{
    // WorldToScreen이 worldPos - cameraPosition이므로, "지금 화면 맨 아래(스크린 y = WINDOW_HEIGHT)"에
    // 해당하는 월드 y는 그 역산인 cameraPosition + WINDOW_HEIGHT다. 카메라가 위로 올라갈수록(음수로
    // 커질수록) 이 값도 같이 작아져서, 데스존이 항상 지금 보이는 화면 바닥에 붙어 따라 올라간다.
    return CameraManager::GetInstance().GetPosition().y + Constants::WINDOW_HEIGHT + Constants::DEATH_ZONE_MARGIN_BELOW_SCREEN;
}

float BlockManager::GetTallestHeightMeters() const
{
    bool hasStackedBlock = false;
	float highestBlockY = 0.0f;

	for (Block* block : GetAllBlocks())
	{
		if (block->GetPhysicsState() == PhysicsState::Airborne)
		{
			continue;
		}
		float blockY = block->GetRenderPosition().y;
		if (!hasStackedBlock || blockY < highestBlockY)
		{
			highestBlockY = blockY;
			hasStackedBlock = true;
		}
	}
	if (!hasStackedBlock)
	{
		return 0.0f;
	}
	float heightInPixels = Constants::FLOOR_TOP_Y - highestBlockY;
	float heightInTiles = heightInPixels / Constants::TILE_SIZE;
    return heightInTiles * Constants::METERS_PER_TILE;
}

bool BlockManager::IsPointInDeathZone(Vector2 worldPoint) const
{
    bool isBelowDeathZoneTop = worldPoint.y >= GetDeathZoneTopY();
    bool isOutsideFloorWidth = worldPoint.x < Constants::FLOOR_LEFT_X || worldPoint.x > Constants::FLOOR_RIGHT_X;
    return isBelowDeathZoneTop && isOutsideFloorWidth;
}

void BlockManager::CheckDeathZone()
{
    if (m_isGameOver)
    {
        return;
    }

    for (Block* block : GetAllBlocks())
    {
        for (int i = 0; i < block->GetCellCount(); ++i)
        {
            Vector2 corners[4];
            block->GetCellRotatedCorners(i, corners);

            for (int c = 0; c < 4; ++c)
            {
                if (IsPointInDeathZone(corners[c]))
                {
                    m_isGameOver = true;
                    return;
                }
            }
        }
    }
}

void BlockManager::RemoveBlock(Block* block)
{
    for (size_t i = 0; i < m_blocks.size(); ++i)
    {
        if (m_blocks[i].get() == block)
        {
            m_blocks.erase(m_blocks.begin() + i);
            return;
        }
    }
}

Block* BlockManager::GetCurrentFallingBlock() const
{
    return m_currentFallingBlock;
}

std::vector<Block*> BlockManager::GetAllBlocks() const
{
    std::vector<Block*> allBlocks;
    allBlocks.reserve(m_blocks.size());
    for (const std::unique_ptr<Block>& block : m_blocks)
    {
        allBlocks.push_back(block.get());
    }
    return allBlocks;
}
