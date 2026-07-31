#pragma once

#include <vector>

class Block;

// 셀 하나의 상태: 0 = 빈 칸, 1 = 점유(바닥 또는 착지한 블럭)
using CellArray = std::vector<std::vector<int>>;

// 바닥 그리드의 셀 점유 상태를 관리하는 싱글톤.
// 좌우 이동을 0.5 테트리스 칸 단위로 다루기 위해 그리드는 서브셀 해상도로 표현한다
// (m_cells 크기는 Constants::GRID_WIDTH_SUBCELLS x GRID_HEIGHT_SUBCELLS).
// Airborne 블럭의 이동/낙하 가능 여부 조회에도 GetCellState를 그대로 재사용한다.
class GridManager
{
public:
    static GridManager& GetInstance();

    // m_cells를 그리드 크기로 초기화하고, 바닥이 차지하는 영역을 미리 점유 칸으로 표시한다
    void Init();

    int GetCellState(int subCellX, int subCellY) const;
    void MarkOccupied(Block* block);

private:
    GridManager() = default;
    ~GridManager() = default;
    GridManager(const GridManager&) = delete;
    GridManager& operator=(const GridManager&) = delete;

    CellArray m_cells;
};
