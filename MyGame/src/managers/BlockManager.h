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

    // 공중 상태(Airborne)인 현재 블럭의 낙하 타이머 갱신. 시간이 되면 Block::StepDown() 호출.
    void UpdateFalling(float deltaTime);

    // 공중 상태인 현재 블럭에 좌우 이동(서브셀 단위)을 요청. Block::MoveHorizontal()로 전달.
    void MoveCurrentBlock(int subCellDelta);

    // 착지 처리: Block::Land() 호출 및 GridManager::MarkOccupied 통지
    void LockBlock(Block* block);

private:
    BlockManager() = default;
    ~BlockManager();
    BlockManager(const BlockManager&) = delete;
    BlockManager& operator=(const BlockManager&) = delete;

    // 블럭의 생명주기를 책임지는 유일한 소유자
    std::vector<std::unique_ptr<Block>> m_blocks;
    Block* m_currentFallingBlock = nullptr;

    // 다음 그리드 낙하 스텝(StepDown)까지 남은 시간
    float m_fallTimer = 0.0f;
};
