#include "pch.h"

#include "GiantTetrominoBlock.h"

// [모양 재사용] TetrominoBlock과 완전히 같은 7종 모양/스프라이트를 그대로 쓴다 — 다른 건 칸 크기(아래
// GetCellSize())와 무게(m_mass)뿐이라, "같은 조각의 확대판"이라는 게 시각적으로도 바로 드러난다.
GiantTetrominoBlock::GiantTetrominoBlock(TetrominoShape shape)
{
    switch (shape)
    {
    case TetrominoShape::I:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 1.0f, 0.0f };
        m_cellShape[2] = { 2.0f, 0.0f };
        m_cellShape[3] = { 3.0f, 0.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/BlockT.png";
        m_colorSlotIndex = 0;
        break;
    case TetrominoShape::O:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 1.0f, 0.0f };
        m_cellShape[2] = { 0.0f, 1.0f };
        m_cellShape[3] = { 1.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_canRotate = false;
        m_spriteId = "assets/BlockT.png";
        m_colorSlotIndex = 1;
        break;
    case TetrominoShape::T:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 1.0f, 0.0f };
        m_cellShape[2] = { 2.0f, 0.0f };
        m_cellShape[3] = { 1.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/BlockT.png";
        m_colorSlotIndex = 2;
        break;
    case TetrominoShape::S:
        m_cellShape[0] = { 1.0f, 0.0f };
        m_cellShape[1] = { 2.0f, 0.0f };
        m_cellShape[2] = { 0.0f, 1.0f };
        m_cellShape[3] = { 1.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/BlockT.png";
        m_colorSlotIndex = 3;
        break;
    case TetrominoShape::Z:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 1.0f, 0.0f };
        m_cellShape[2] = { 1.0f, 1.0f };
        m_cellShape[3] = { 2.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/BlockT.png";
        m_colorSlotIndex = 4;
        break;
    case TetrominoShape::J:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 0.0f, 1.0f };
        m_cellShape[2] = { 1.0f, 1.0f };
        m_cellShape[3] = { 2.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/BlockT.png";
        m_colorSlotIndex = 5;
        break;
    case TetrominoShape::L:
        m_cellShape[0] = { 2.0f, 0.0f };
        m_cellShape[1] = { 0.0f, 1.0f };
        m_cellShape[2] = { 1.0f, 1.0f };
        m_cellShape[3] = { 2.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/BlockT.png";
        m_colorSlotIndex = 6;
        break;
    }

    // [무게 스케일링 — 사용자 요청] 커진 크기 비율(m_scaleMultiplier)만큼 그대로 무거워진다.
    m_mass = Constants::GIANT_BLOCK_MASS_MULTIPLIER;
}

float GiantTetrominoBlock::GetCellSize() const
{
    return Constants::TILE_SIZE * m_scaleMultiplier;
}
