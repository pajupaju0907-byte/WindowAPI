#include "pch.h"

#include "GridManager.h"
#include "../objects/Block.h"
#include "../util/Constants.h"

GridManager& GridManager::GetInstance()
{
    static GridManager instance;
    return instance;
}

void GridManager::Init()
{
    m_cells.assign(Constants::GRID_HEIGHT_SUBCELLS, std::vector<int>(Constants::GRID_WIDTH_SUBCELLS, 0));

    // 바닥이 차지하는 서브셀 범위를 미리 점유(1) 표시해둔다.
    // 가로: GRID_WIDTH칸 중앙에 FLOOR_WIDTH_TILES칸짜리 발판이 놓인 것과 같은 계산을 서브셀 단위로 재현
    // 세로: 맨 아래 FLOOR_HEIGHT_TILES칸
    int floorLeftSubcell = (Constants::GRID_WIDTH - Constants::FLOOR_WIDTH_TILES) / 2 * Constants::GRID_SUBCELL_SCALE;
    int floorRightSubcell = floorLeftSubcell + Constants::FLOOR_WIDTH_TILES * Constants::GRID_SUBCELL_SCALE;
    int floorTopSubcell = Constants::GRID_HEIGHT_SUBCELLS - Constants::FLOOR_HEIGHT_TILES * Constants::GRID_SUBCELL_SCALE;

    for (int y = floorTopSubcell; y < Constants::GRID_HEIGHT_SUBCELLS; ++y)
    {
        for (int x = floorLeftSubcell; x < floorRightSubcell; ++x)
        {
            m_cells[y][x] = 1;
        }
    }
}

int GridManager::GetCellState(int subCellX, int subCellY) const
{
    // 그리드 범위 밖은 막힌 것으로 취급 (범위 밖으로 못 나가게)
    if (subCellX < 0 || subCellX >= Constants::GRID_WIDTH_SUBCELLS ||
        subCellY < 0 || subCellY >= Constants::GRID_HEIGHT_SUBCELLS)
    {
        return 1;
    }

    return m_cells[subCellY][subCellX];
}

void GridManager::MarkOccupied(Block* block)
{
    // TODO: 지금은 테스트용 1칸짜리 블럭 기준으로 원점 칸만 표시.
    // 실제 테트로미노 모양(cellShape)이 생기면 블럭이 차지하는 모든 칸을 순회하며 표시하도록 확장할 것
    if (block == nullptr)
    {
        return;
    }

    Vector2 pos = block->GetRenderPosition();
    int subCellX = static_cast<int>(pos.x / Constants::SUBCELL_SIZE);
    int subCellY = static_cast<int>(pos.y / Constants::SUBCELL_SIZE);

    if (subCellX >= 0 && subCellX < Constants::GRID_WIDTH_SUBCELLS &&
        subCellY >= 0 && subCellY < Constants::GRID_HEIGHT_SUBCELLS)
    {
        m_cells[subCellY][subCellX] = 1;
    }
}
