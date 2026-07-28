#pragma once

#include <memory>
#include <vector>
#include "../util/Types.h"

class Block;

// 블럭의 생성과 소유, 낙하 중인 블럭의 고정(Lock)을 담당하는 싱글톤
class BlockManager
{
public:
    static BlockManager& GetInstance();

    void SpawnBlock(BlockType type);
    void LockBlock(Block* block);

private:
    BlockManager() = default;
    ~BlockManager();
    BlockManager(const BlockManager&) = delete;
    BlockManager& operator=(const BlockManager&) = delete;

    // 블럭의 생명주기를 책임지는 유일한 소유자
    std::vector<std::unique_ptr<Block>> m_blocks;
    Block* m_currentFallingBlock = nullptr;
};
