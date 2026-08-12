#include "pch.h"

#include "TetrominoBlock.h"

TetrominoBlock::TetrominoBlock(TetrominoShape shape)
{
    // TODO: 콜라이더 생성 직접 구현
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
}
