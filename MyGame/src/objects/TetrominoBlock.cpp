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
        m_spriteId = "assets/block1.png";
        break;
    case TetrominoShape::O:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 1.0f, 0.0f };
        m_cellShape[2] = { 0.0f, 1.0f };
        m_cellShape[3] = { 1.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_canRotate = false;
        m_spriteId = "assets/block2.png";
        break;
    case TetrominoShape::T:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 1.0f, 0.0f };
        m_cellShape[2] = { 2.0f, 0.0f };
        m_cellShape[3] = { 1.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/block3.png";
        break;
    case TetrominoShape::S:
        m_cellShape[0] = { 1.0f, 0.0f };
        m_cellShape[1] = { 2.0f, 0.0f };
        m_cellShape[2] = { 0.0f, 1.0f };
        m_cellShape[3] = { 1.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/block4.png";
        break;
    case TetrominoShape::Z:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 1.0f, 0.0f };
        m_cellShape[2] = { 1.0f, 1.0f };
        m_cellShape[3] = { 2.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/block5.png";
        break;
    case TetrominoShape::J:
        m_cellShape[0] = { 0.0f, 0.0f };
        m_cellShape[1] = { 0.0f, 1.0f };
        m_cellShape[2] = { 1.0f, 1.0f };
        m_cellShape[3] = { 2.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/block6.png";
        break;
    case TetrominoShape::L:
        m_cellShape[0] = { 2.0f, 0.0f };
        m_cellShape[1] = { 0.0f, 1.0f };
        m_cellShape[2] = { 1.0f, 1.0f };
        m_cellShape[3] = { 2.0f, 1.0f };
        m_pivot = { 1.0f, 0.0f };
        m_spriteId = "assets/block7.png";
        break;
    }
}
