#pragma once

// 매직 넘버 금지 원칙에 따른 게임 전역 상수 모음.
// TODO: 아래 값들은 자리표시자(placeholder)이며, 실제 체감/밸런스에 맞게 직접 조정할 것.
namespace Constants
{
    constexpr int GRID_WIDTH = 10;
    constexpr int GRID_HEIGHT = 20;

    // 48px 타일 * 10x20 그리드 = 480x960 창 크기 
    constexpr float TILE_SIZE = 48.0f;

    constexpr int WINDOW_WIDTH = static_cast<int>(GRID_WIDTH * TILE_SIZE);
    constexpr int WINDOW_HEIGHT = static_cast<int>(GRID_HEIGHT * TILE_SIZE);

    // GRAVITY는 바닥 상태(Awake)의 흔들림 물리 전용. 공중 낙하(Airborne)는 그리드 스텝으로 처리하므로 사용하지 않는다.
    constexpr float GRAVITY = 9.8f;

    // 좌우 이동을 테트리스 1칸의 절반 단위로 다루기 위해, 그리드를 서브셀 해상도로 재정의한다.
    // 1 테트리스 칸 = 서브셀 2칸. 낙하는 서브셀 2칸(=1칸)씩, 좌우 이동은 서브셀 1칸(=0.5칸)씩 움직인다.
    constexpr int GRID_SUBCELL_SCALE = 2;
    constexpr int GRID_WIDTH_SUBCELLS = GRID_WIDTH * GRID_SUBCELL_SCALE;
    constexpr int GRID_HEIGHT_SUBCELLS = GRID_HEIGHT * GRID_SUBCELL_SCALE;
    constexpr float SUBCELL_SIZE = TILE_SIZE / GRID_SUBCELL_SCALE;

    constexpr int MOVE_STEP_SUBCELLS = 1;
    constexpr int FALL_STEP_SUBCELLS = GRID_SUBCELL_SCALE;

    // TODO: 한 칸 낙하까지 걸리는 시간(초). 실제 체감/밸런스에 맞게 직접 조정할 것
    constexpr float FALL_STEP_INTERVAL = 0.0f;

    //격자선 색상 ( 연한 회색 )
    constexpr COLORREF GRID_LINE_COLOR = RGB(220, 220, 220);
    //블록 타일 색상 (파란색 ) 
	constexpr COLORREF BLOCK_TILE_COLOR = RGB(30, 100, 240);
}
