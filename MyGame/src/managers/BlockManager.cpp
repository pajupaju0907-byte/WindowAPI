#include "pch.h"

#include "BlockManager.h"
#include "../objects/Block.h"
#include "../objects/TetrominoBlock.h"
#include "../objects/GiantTetrominoBlock.h"
#include "../objects/HeavyBlock.h"
#include "../util/Constants.h"
#include "GridManager.h"
#include "../core/InputManager.h"
#include <cstdlib>

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

	float fallSpeedMultiplier = InputManager::GetInstance().IsKeyDown(VK_DOWN) ? Constants::SOFT_DROP_MULTIPLIER : 1.0f;
	m_fallTimer -= deltaTime * fallSpeedMultiplier;

    if (m_fallTimer <= 0.0f)
    {
		bool moved = m_currentFallingBlock->StepDown();
		m_fallTimer += Constants::FALL_STEP_INTERVAL;

        if (!moved)
        {
            LockBlock(m_currentFallingBlock);
            m_currentFallingBlock = nullptr;
            SpawnBlock(BlockType::Tetromino);
        }
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
    block->MarkOccupiedCells();
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
