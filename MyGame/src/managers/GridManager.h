#pragma once

#include <vector>

class Block;

// TODO: 셀 하나의 상태를 int로 임시 표현. 실제 표현 방식(점유 여부, 블럭 참조 등)은 직접 설계할 것
using CellArray = std::vector<std::vector<int>>;

// 바닥 그리드의 셀 점유 상태를 관리하는 싱글톤
class GridManager
{
public:
    static GridManager& GetInstance();

    int GetCellState(int x, int y) const;
    void MarkOccupied(Block* block);

private:
    GridManager() = default;
    ~GridManager() = default;
    GridManager(const GridManager&) = delete;
    GridManager& operator=(const GridManager&) = delete;

    CellArray m_cells;
};
