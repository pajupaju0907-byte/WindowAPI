#pragma once

// 매직 넘버 금지 원칙에 따른 게임 전역 상수 모음.
// TODO: 아래 값들은 자리표시자(placeholder)이며, 실제 체감/밸런스에 맞게 직접 조정할 것.
namespace Constants
{
    constexpr int GRID_WIDTH = 10;
    constexpr int GRID_HEIGHT = 20;
    constexpr float TILE_SIZE = 32.0f;
    constexpr float GRAVITY = 9.8f;
}
