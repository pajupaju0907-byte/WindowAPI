#pragma once

#include "Block.h"
#include "../util/Types.h"
#include "../util/Constants.h"

// 일반 테트로미노(4칸)와 모양/칸 개수는 완전히 같고, 칸 하나하나의 실제 크기와 무게만 더 큰 블럭.
class GiantTetrominoBlock : public Block
{
public:
    GiantTetrominoBlock(TetrominoShape shape);

    // [칸 크기 오버라이드] Block의 기본 TILE_SIZE 대신, 확대 배율을 곱한 크기를 돌려준다 —
    // 이 값 하나가 렌더링/충돌/관성모멘트/무게중심 계산 전체에 퍼져나간다(Block::GetCellSize() 주석 참고).
    float GetCellSize() const override;
    bool IsGiant() const override { return true; }

private:
    float m_scaleMultiplier = Constants::GIANT_BLOCK_SCALE_MULTIPLIER;
};
