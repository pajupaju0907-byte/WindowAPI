#pragma once

#include "Block.h"

// 일반 테트로미노보다 크게 확대된 블럭
class GiantTetrominoBlock : public Block
{
public:
    GiantTetrominoBlock();

private:
    // TODO: 실제 확대 배율 값은 직접 설계할 것
    float m_scaleMultiplier = 1.0f;
};
