#include "GridManager.h"

GridManager& GridManager::GetInstance()
{
    static GridManager instance;
    return instance;
}

int GridManager::GetCellState(int x, int y) const
{
    // TODO: m_cells 조회 로직 직접 구현
    (void)x;
    (void)y;
    return 0;
}

void GridManager::MarkOccupied(Block* block)
{
    // TODO: 블럭이 차지하는 셀들을 m_cells에 표시하는 로직 직접 구현
    (void)block;
}
