#include "pch.h"

#include "BlockManager.h"
#include "../objects/Block.h"

BlockManager& BlockManager::GetInstance()
{
    static BlockManager instance;
    return instance;
}

BlockManager::~BlockManager() = default;

void BlockManager::SpawnBlock(BlockType type)
{
    // TODO: type에 맞는 파생 Block 생성 후 m_blocks에 등록, currentFallingBlock 갱신 직접 구현
    (void)type;
}

void BlockManager::UpdateFalling(float deltaTime)
{
    // TODO: m_fallTimer를 deltaTime만큼 줄이다가 Constants::FALL_STEP_INTERVAL이 되면
    // m_currentFallingBlock->StepDown() 호출하고 타이머 재설정하는 로직 직접 구현
    (void)deltaTime;
}

void BlockManager::MoveCurrentBlock(int subCellDelta)
{
    // TODO: m_currentFallingBlock->MoveHorizontal(subCellDelta) 호출 직접 구현
    (void)subCellDelta;
}

void BlockManager::LockBlock(Block* block)
{
    // TODO: 낙하 종료 처리(Block::Land() 호출, GridManager::MarkOccupied 통지 등) 직접 구현
    (void)block;
}
