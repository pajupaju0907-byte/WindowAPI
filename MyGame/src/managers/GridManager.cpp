#include "pch.h"

#include "GridManager.h"

GridManager& GridManager::GetInstance()
{
    static GridManager instance;
    return instance;
}

int GridManager::GetCellState(int subCellX, int subCellY) const
{
    // TODO: m_cells(서브셀 좌표 기준) 조회 로직 직접 구현
    (void)subCellX;
    (void)subCellY;
    return 0;
}

void GridManager::MarkOccupied(Block* block)
{
    // TODO: 블럭이 차지하는 서브셀들을 m_cells에 표시하는 로직 직접 구현
    (void)block;
}
