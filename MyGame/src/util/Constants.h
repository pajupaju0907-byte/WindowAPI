#pragma once

// 매직 넘버 금지 원칙에 따른 게임 전역 상수 모음.
// TODO: 아래 값들은 자리표시자(placeholder)이며, 실제 체감/밸런스에 맞게 직접 조정할 것.
namespace Constants
{
    constexpr int GRID_WIDTH = 14;
    constexpr int GRID_HEIGHT = 20;

    // 48px 타일 * 14x20 그리드 = 672x960 창 크기
    constexpr float TILE_SIZE = 48.0f;

    //발판 사이즈
    constexpr int FLOOR_WIDTH_TILES = 8;
    constexpr int FLOOR_HEIGHT_TILES = 3;

    // 바닥(발판) 윗면의 픽셀 Y좌표. 착지한 블럭이 이보다 아래로 파고들면 안 됨
    constexpr float FLOOR_TOP_Y = (GRID_HEIGHT - FLOOR_HEIGHT_TILES) * TILE_SIZE;

    constexpr int WINDOW_WIDTH = static_cast<int>(GRID_WIDTH * TILE_SIZE);
    constexpr int WINDOW_HEIGHT = static_cast<int>(GRID_HEIGHT * TILE_SIZE);

    // GRAVITY는 바닥 상태(Awake)의 흔들림 물리 전용. 공중 낙하(Airborne)는 그리드 스텝으로 처리하므로 사용하지 않는다.
    constexpr float GRAVITY = 900.0f;

    // 이 속도(픽셀/초)를 넘는 Awake 블럭이 하나라도 있으면 탑이 불안정하다고 판단. 체감에 맞게 조정할 것
    constexpr float UNSTABLE_SPEED_THRESHOLD = 500.0f;

    // 바닥/다른 블럭에 파고들었다 밀려날 때, 속도를 0으로 죽이는 대신 이 비율만큼만 남기고 반대로 튕겨서
    // 서서히 잦아드는 흔들림(wobble)을 만든다. 1.0에 가까울수록 오래 튕기고, 0에 가까울수록 즉시 멈춤
    constexpr float BOUNCE_RESTITUTION = 0.3f;

    // 좌우 이동을 테트리스 1칸의 절반 단위로 다루기 위해, 그리드를 서브셀 해상도로 재정의한다.
    // 1 테트리스 칸 = 서브셀 2칸. 낙하는 서브셀 2칸(=1칸)씩, 좌우 이동은 서브셀 1칸(=0.5칸)씩 움직인다.
    constexpr int GRID_SUBCELL_SCALE = 2;
    constexpr int GRID_WIDTH_SUBCELLS = GRID_WIDTH * GRID_SUBCELL_SCALE;
    constexpr int GRID_HEIGHT_SUBCELLS = GRID_HEIGHT * GRID_SUBCELL_SCALE;
    constexpr float SUBCELL_SIZE = TILE_SIZE / GRID_SUBCELL_SCALE;

    constexpr int MOVE_STEP_SUBCELLS = 1;

    // 낙하도 좌우 이동처럼 서브셀 1칸(=0.5칸)씩 내려가도록. 전체 낙하 속도는 유지하려고
    // FALL_STEP_INTERVAL도 기존(1칸=2서브셀당 0.8초) 대비 절반인 0.4초로 같이 줄임 — 속도가 원하던 것과 다르면 조정할 것
    constexpr int FALL_STEP_SUBCELLS = 1;

    // 한 서브셀 낙하까지 걸리는 시간(초).
    constexpr float FALL_STEP_INTERVAL = 0.4f;

    // 아래키를 누르고 있을 때(소프트 드롭) 낙하 타이머가 줄어드는 배율. 클수록 빨리 떨어짐.
    constexpr float SOFT_DROP_MULTIPLIER = 8.0f;

    // 좌우 이동 키를 누르고 있을 때 반복 이동 간격(초). 처음 누른 순간은 즉시 반응하고, 그 뒤로 이 간격마다 반복.
    constexpr float MOVE_REPEAT_INTERVAL = 0.1f;

    //격자선 색상 ( 연한 회색 )
    constexpr COLORREF GRID_LINE_COLOR = RGB(220, 220, 220);
    //블록 타일 색상 (파란색 )
	constexpr COLORREF BLOCK_TILE_COLOR = RGB(30, 100, 240);

    // 프레임 제한: 목표 FPS와, 그로부터 계산한 한 프레임당 목표 시간(초)
    constexpr int TARGET_FPS = 60;
    constexpr double TARGET_FRAME_SECONDS = 1.0 / TARGET_FPS;

    // FPS 표시(디버그용) 위치/색상
    constexpr int FPS_TEXT_MARGIN = 8;
    constexpr COLORREF FPS_TEXT_COLOR = RGB(255, 0, 0);
}
