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

    // 바닥(발판)의 왼쪽/오른쪽 끝 픽셀 X좌표 (GRID_WIDTH 중앙에 FLOOR_WIDTH_TILES폭 발판이 놓인 위치).
    // 발판 옆은 빈 공간이라, 바닥 지지/충돌 판정에서 Y좌표뿐 아니라 이 X범위 안인지도 같이 확인해야 한다 —
    // 안 그러면 발판 옆 허공에 떠 있어도 "바닥에 닿았다"고 잘못 판단하게 된다
    constexpr float FLOOR_LEFT_X = (GRID_WIDTH - FLOOR_WIDTH_TILES) / 2.0f * TILE_SIZE;
    constexpr float FLOOR_RIGHT_X = FLOOR_LEFT_X + FLOOR_WIDTH_TILES * TILE_SIZE;

    constexpr int WINDOW_WIDTH = static_cast<int>(GRID_WIDTH * TILE_SIZE);
    constexpr int WINDOW_HEIGHT = static_cast<int>(GRID_HEIGHT * TILE_SIZE);

    // GRAVITY는 바닥 상태(Awake)의 흔들림 물리 전용. 공중 낙하(Airborne)는 그리드 스텝으로 처리하므로 사용하지 않는다.
    constexpr float GRAVITY = 900.0f;

    // 이 속도(픽셀/초)를 넘는 Awake 블럭이 하나라도 있으면 탑이 불안정하다고 판단. 체감에 맞게 조정할 것
    constexpr float UNSTABLE_SPEED_THRESHOLD = 500.0f;

    // 이 각속도(도/초)를 넘는 Awake 블럭이 하나라도 있으면 탑이 불안정하다고 판단 (회전판 UNSTABLE_SPEED_THRESHOLD)
    constexpr float UNSTABLE_ANGULAR_SPEED_THRESHOLD = 30.0f;

    // 바닥/다른 블럭에 파고들었다 밀려날 때, 속도를 0으로 죽이는 대신 이 비율만큼만 남기고 반대로 튕겨서
    // 서서히 잦아드는 흔들림(wobble)을 만든다. 1.0에 가까울수록 오래 튕기고, 0에 가까울수록 즉시 멈춤.
    // 블럭은 탱탱볼이 아니라 콘크리트/나무토막에 가까워야 해서 0에 아주 가깝게 잡는다
    constexpr float BOUNCE_RESTITUTION = 0.02f;

    // [위치 보정] 파고든 깊이 중 이 정도(px)는 그냥 무시한다("slop"). 0까지 완벽하게 밀어내려고 하면
    // 미세한 파고듦-보정-재파고듦이 반복되며 계속 떨리는데, 아주 약간의 파고듦은 그냥 봐줘서 그 진동을 끊는다
    constexpr float POSITION_CORRECTION_SLOP = 1.0f;

    // [위치 보정] slop을 뺀 파고든 깊이 중 실제로 밀어낼 비율(0~1). 한 번에 100% 다 밀어내면 그 반동으로
    // 쌓인 블럭들이 스프링처럼 서로 밀어내며 꿀렁거릴 수 있어서, 여러 프레임(솔버 반복)에 걸쳐 나눠서 민다
    constexpr float POSITION_CORRECTION_PERCENT = 0.6f;

    // [마찰] 접촉면을 따라 미끄러지는 걸 붙잡는 세기. 마찰 임펄스는 이 값 * 수직 방향 임펄스 크기를
    // 못 넘도록 clamp된다(쿨롱 마찰 모델) — 실제 마찰처럼 세게 눌려있을수록 더 세게 붙잡을 수 있다는 뜻.
    // 0이면 마찰 없음(예전 상태), 1에 가까울수록 잘 안 미끄러짐
    constexpr float FRICTION_COEFFICIENT = 0.7f;

    // 무게중심 판정에서, 바닥/다른 블럭 바로 위에 얹혀 있다고 인정할 허용 오차(px)
    constexpr float SUPPORT_CHECK_TOLERANCE = 4.0f;

    // 무게중심이 지지 범위를 이 정도(px) 넘게 벗어나야 "불안정"으로 판단하는 여유값(오차 무시용)
    constexpr float IMBALANCE_DEADZONE = 2.0f;

    // [강제 취침 타임아웃] 지지대가 있는 블럭이 미세한 진동 때문에 속도가 계속 임계값을 살짝 넘나들어
    // TrySleepAll의 정상 판정으로는 영원히 안 잠드는 경우를 위한 안전장치. Awake/Toppling 상태로
    // 이 시간(초)을 넘기면, 지지대가 있는 한 속도와 무관하게 강제로 Sleeping 처리한다.
    constexpr float FORCE_SLEEP_TIMEOUT = 3.0f;

    // [각도 스냅] Sleep()에서 90도 배수와의 차이가 이 값(도) 이내일 때만 각도를 반올림한다.
    // 항상 반올림하면, 다른 블럭에 기대서 진짜로 비스듬히 안정된 블럭까지 강제로 우뚝 세워버리게 된다 —
    // 미세한 물리 오차만 보정하고, 확실히 기울어진 채로 멈춘 건 그대로 둔다
    constexpr float ANGLE_SNAP_TOLERANCE = 12.0f;

    // [바닥 마찰 근사] 지지대가 있는(바닥/다른 블럭 위에 얹힌) Awake/Toppling 블럭에 매 프레임 곱하는
    // 속도 감쇠 비율. ResolveRigidCollision의 임펄스 기반 마찰은 얹혀서 미끄러지는 동안(수직 속도 거의 0)엔
    // 전혀 안 걸리는 구조적 한계가 있어서, 이 감쇠로 "바닥 마찰"을 흉내낸다. 1.0에 가까울수록 안 미끄러지고
    // 멈춤, 0에 가까울수록 잘 미끄러짐
    constexpr float GROUNDED_VELOCITY_DAMPING = 0.75f;

    // [무게중심 보조 토크] 순수 물리(중력+접촉점 충돌)만으로 무게중심 벗어난 블럭을 넘어뜨려봤더니
    // 마찰/관성 때문에 너무 약하고 느려서 실제로는 안 넘어갔다 — 그래서 무게중심이 지지 범위를
    // 1px 벗어날 때마다 이만큼의 "기준" 각가속도(도/초^2, REFERENCE_MOMENT_OF_INERTIA 기준)를 보조로
    // 더해준다. 실제 회전 반응(충돌 임펄스)을 대체하는 게 아니라 그게 시작되도록 살짝 떠미는 역할.
    // 균형이 다시 맞았을 때 억지로 감쇠시키진 않는다 — 그건 마찰/충돌 같은 실제 물리에 맡긴다
    constexpr float TOPPLE_ANGULAR_ACCELERATION = 20.0f;

    // 위 TOPPLE_ANGULAR_ACCELERATION 값을 조정한 "기준" 관성모멘트. 실제 블럭의 관성모멘트가 이보다 크면
    // (I자처럼 길쭉해서 회전에 더 잘 버티는 모양) 그 비율만큼 덜 가속되고, 작으면(O자처럼 뭉친 모양) 더 가속된다
    constexpr float REFERENCE_MOMENT_OF_INERTIA = 2000.0f;

    // 무게중심이 이 각도(도)까지 기울면 더 못 버티고 완전히 무너지는 것으로 확정한다 (Awake -> Toppling 전환).
    // 각속도가 보조 토크에서 왔든 실제 충돌에서 왔든 상관없이, 이 각도를 넘으면 무조건 여기서 걸린다 —
    // 그래서 무한 회전 걱정 없이 위 보조 토크를 마음 놓고 쓸 수 있다.
    // Toppling 전환 이후엔 속도를 인위적으로 바꾸지 않고 실제 물리(중력+충돌)만으로 움직인다
    constexpr float MAX_TOPPLE_ANGLE = 40.0f;

    // [강체물리 4단계] 한 프레임 안에서 충돌 해소(바닥+블럭끼리)를 몇 번 반복할지.
    // 블럭이 여러 개 쌓이면 한 쌍을 풀다가 다른 쌍이 다시 겹치는 일이 생기는데, 여러 번 반복해주면
    // 그때마다 조금씩 덜 겹치는 쪽으로 수렴해서 안정적으로 보인다. 너무 크면 그만큼 느려짐
    constexpr int COLLISION_SOLVER_ITERATIONS = 4;

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

    // [락 딜레이] 낙하 중인 블럭이 더 못 내려가는 상태로 이 시간(초) 이상 버티면 그때 착지(Lock) 확정한다.
    // 막힌 즉시 Lock하면, 좁은 틈으로 옆으로 비켜 넣으려는 도중에도 락이 걸려버려서 못 들어간다.
    constexpr float LOCK_DELAY_DURATION = 0.5f;

    // 아래키를 누르고 있을 때(소프트 드롭) 낙하 타이머가 줄어드는 배율. 클수록 빨리 떨어짐.
    constexpr float SOFT_DROP_MULTIPLIER = 8.0f;

    // 좌우 이동 키를 누르고 있을 때 반복 이동 간격(초). 처음 누른 순간은 즉시 반응하고, 그 뒤로 이 간격마다 반복.
    constexpr float MOVE_REPEAT_INTERVAL = 0.1f;

    //격자선 색상 ( 연한 회색 )
    constexpr COLORREF GRID_LINE_COLOR = RGB(220, 220, 220);
    //블록 타일 색상 (파란색 )
	constexpr COLORREF BLOCK_TILE_COLOR = RGB(30, 100, 240);
    // 디버그용 콜라이더 표시 색상 (빨간색), F1로 토글
    constexpr COLORREF COLLIDER_DEBUG_COLOR = RGB(255, 0, 0);

    // 프레임 제한: 목표 FPS와, 그로부터 계산한 한 프레임당 목표 시간(초)
    constexpr int TARGET_FPS = 60;
    constexpr double TARGET_FRAME_SECONDS = 1.0 / TARGET_FPS;

    // FPS 표시(디버그용) 위치/색상
    constexpr int FPS_TEXT_MARGIN = 8;
    constexpr COLORREF FPS_TEXT_COLOR = RGB(255, 0, 0);
}
