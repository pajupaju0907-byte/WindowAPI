#include "pch.h"

#include "TetrominoBlock.h"

TetrominoBlock::TetrominoBlock()
{
    // TODO: 콜라이더 생성 직접 구현. 나머지 I/S/Z/T/J/L 모양도 직접 설계할 것 (지금은 O자만)
    m_spriteId = "assets/block1.png";

    // O자 모양 (2x2, 테트리스 칸 단위)
    m_cellShape[0] = { 0.0f, 0.0f };
    m_cellShape[1] = { 1.0f, 0.0f };
    m_cellShape[2] = { 0.0f, 1.0f };
    m_cellShape[3] = { 1.0f, 1.0f };
}
