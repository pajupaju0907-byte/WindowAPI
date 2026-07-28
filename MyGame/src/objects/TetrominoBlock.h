#pragma once

#include "Block.h"

// 4칸으로 구성된 일반 테트로미노 블럭 (I/O/T/S/Z/J/L 등)
class TetrominoBlock : public Block
{
public:
    TetrominoBlock();

private:
    // TODO: 실제 테트로미노 모양(4칸의 상대 좌표) 정의는 직접 설계할 것
    Vector2 m_cellShape[4];
};
