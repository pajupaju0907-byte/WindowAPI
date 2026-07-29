#pragma once

#include <vector>

class Block;

// TODO: 셀 하나의 상태를 int로 임시 표현. 실제 표현 방식(점유 여부, 블럭 참조 등)은 직접 설계할 것
using CellArray = std::vector<std::vector<int>>;

// 바닥 그리드의 셀 점유 상태를 관리하는 싱글톤.
// 좌우 이동을 0.5 테트리스 칸 단위로 다루기 위해 그리드는 서브셀 해상도로 표현한다
// (m_cells 크기는 Constants::GRID_WIDTH_SUBCELLS x GRID_HEIGHT_SUBCELLS).
// Airborne 블럭의 이동/낙하 가능 여부 조회에도 GetCellState를 그대로 재사용한다.
class GridManager
{
public:
    static GridManager& GetInstance();

    int GetCellState(int subCellX, int subCellY) const;
    void MarkOccupied(Block* block);

private:
    GridManager() = default;
    ~GridManager() = default;
    GridManager(const GridManager&) = delete;
    GridManager& operator=(const GridManager&) = delete;

    CellArray m_cells;
};
