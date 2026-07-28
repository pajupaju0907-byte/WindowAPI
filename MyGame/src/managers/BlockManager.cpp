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

void BlockManager::LockBlock(Block* block)
{
    // TODO: 낙하 종료 처리(PhysicsState::Locked 전환, GridManager 통지 등) 직접 구현
    (void)block;
}
