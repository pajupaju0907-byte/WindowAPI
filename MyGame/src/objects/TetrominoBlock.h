#pragma once

#include "Block.h"

// 4칸으로 구성된 일반 테트로미노 블럭 (I/O/T/S/Z/J/L 등)
class TetrominoBlock : public Block
{
public:
	TetrominoBlock(TetrominoShape shape);
};
