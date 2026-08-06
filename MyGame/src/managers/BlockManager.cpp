#include "pch.h"

#include "BlockManager.h"
#include "../objects/Block.h"
#include "../objects/TetrominoBlock.h"
#include "../objects/GiantTetrominoBlock.h"
#include "../objects/HeavyBlock.h"
#include "../util/Constants.h"
#include "../core/InputManager.h"
#include <cstdlib>
#include "PhysicsManager.h"
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

    // 그리드 가로 중앙, 맨 위 줄에서 스폰 시작
    spawnedBlock->SetGridPosition(Constants::GRID_WIDTH_SUBCELLS / 2, 0);

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
    PhysicsManager::GetInstance().WakeAll();
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
