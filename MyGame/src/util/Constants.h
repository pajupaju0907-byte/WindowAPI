#pragma once

// 매직 넘버 금지 원칙에 따른 게임 전역 상수 모음.
// TODO: 아래 값들은 자리표시자(placeholder)이며, 실제 체감/밸런스에 맞게 직접 조정할 것.
namespace Constants
{
	constexpr int GRID_WIDTH = 14;
	constexpr int GRID_HEIGHT = 20;

	// 48px 타일 * 14x20 그리드 = 672x960 창 크기
	constexpr float TILE_SIZE = 48.0f;

	// [블럭 색상] BlockT.png는 가로로 색깔 정사각형 11칸이 이어진 스프라이트시트(칸 사이 여백 있음).
	// 테트로미노 7종은 이 중 앞에서부터 7칸(0~6)을 하나씩 잘라 쓴다(TetrominoBlock::TetrominoBlock 참고).
	// 실제 정사각형 그림은 이미지(1536x1024) 전체가 아니라 세로 가운데의 좁은 띠 안에만 있고 위아래는
	// 거대한 여백이라, 세로를 이미지 전체 높이로 잡으면 정사각형 타일에 그릴 때 심하게 찌부러진다.
	// 그래서 실제 그림이 있는 영역(픽셀 단위로 측정한 값)만 잘라 쓴다.
	constexpr float BLOCK_COLOR_SHEET_CONTENT_LEFT = 25.0f;
	constexpr float BLOCK_COLOR_SHEET_CONTENT_TOP = 428.0f;
	constexpr float BLOCK_COLOR_SHEET_SLOT_WIDTH = 135.3f;
	constexpr float BLOCK_COLOR_SHEET_SLOT_HEIGHT = 145.0f;

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

	constexpr COLORREF DEATH_ZONE_COLOR = RGB(255, 0, 0);
	constexpr float DEATH_ZONE_OPACITY = 0.35f;

	// [낙하 가이드] 낙하 중인 블럭이 착지할 위치까지의 경로를 반투명 흰색으로 표시한다. 블럭보다 먼저(뒤에) 그린다.
	constexpr COLORREF DROP_GUIDE_COLOR = RGB(255, 255, 255);
	constexpr float DROP_GUIDE_OPACITY = 0.25f;

	constexpr int WINDOW_WIDTH = static_cast<int>(GRID_WIDTH * TILE_SIZE);
	constexpr int WINDOW_HEIGHT = static_cast<int>(GRID_HEIGHT * TILE_SIZE);

	// [데스존] 발판 옆(FLOOR_LEFT_X 왼쪽 / FLOOR_RIGHT_X 오른쪽), DEATH_ZONE_TOP_Y 아래 영역에 블록의
	// 칸이 하나라도 닿으면 즉시 게임오버로 처리한다(BlockManager::CheckDeathZone 참고).
	// FLOOR_TOP_Y와 일부러 분리해뒀다 — 발판 높이에 묶여있으면 옆벽에 살짝 스치기만 해도 죽어버린다.
	// 기본값(WINDOW_HEIGHT, 창 맨 아래)은 "화면 밖으로 완전히 벗어나야만" 죽게 한다는 뜻. 더 일찍
	// 죽게 하고 싶으면(=화면 안에서도 죽게) 이 값을 줄이고, 더 깊이 떨어져야 죽게 하고 싶으면 늘린다.
	// [중요] 카메라가 올라가도 항상 "지금 보이는 화면의 바닥"을 기준으로 삼아야 하므로, 고정된 상수가
	// 아니라 매 프레임 카메라 위치 + WINDOW_HEIGHT + 이 여유값으로 계산한다
	// (BlockManager::GetDeathZoneTopY 참고) — 안 그러면 탑이 자라 카메라가 스크롤될수록 데스존이
	// 원래 자리에 그대로 남아 화면 밖으로 사라지고, 블럭이 거기까지 떨어지는 게 사실상 불가능해진다.
	constexpr float DEATH_ZONE_MARGIN_BELOW_SCREEN = 0.0f;

	// [데스존] 화면에 표시할 때 그릴 세로 높이(px). 실제 판정(데스존 시작 지점 아래 전부)과는 별개로,
	// 눈에 띄게 보여줄 만큼만 그린다.
	constexpr float DEATH_ZONE_VISUAL_HEIGHT = TILE_SIZE * 3.0f;

	// GRAVITY는 바닥 상태(Awake)의 흔들림 물리 전용. 공중 낙하(Airborne)는 그리드 스텝으로 처리하므로 사용하지 않는다.
	constexpr float GRAVITY = 900.0f;

	// 이 속도(픽셀/초)를 넘는 Awake 블럭이 하나라도 있으면 탑이 불안정하다고 판단. 체감에 맞게 조정할 것
	constexpr float UNSTABLE_SPEED_THRESHOLD = 500.0f;

	// 이 각속도(도/초)를 넘는 Awake 블럭이 하나라도 있으면 탑이 불안정하다고 판단 (회전판 UNSTABLE_SPEED_THRESHOLD)
	constexpr float UNSTABLE_ANGULAR_SPEED_THRESHOLD = 30.0f;

	// [블록별 rest timer] 이 속도(px/s) 밑으로 계속 유지돼야 Sleep 후보로 본다. UNSTABLE_SPEED_THRESHOLD보다
	// 훨씬 작아야 한다 — 저건 "탑이 무너지고 있는가"를 보는 값이고, 이건 "이 블록 하나가 진짜 멈췄는가"를 보는 값이다.
	constexpr float SLEEP_LINEAR_THRESHOLD = 20.0f;

	// 위 SLEEP_LINEAR_THRESHOLD의 각속도 버전(도/초)
	constexpr float SLEEP_ANGULAR_THRESHOLD = 5.0f;

	// 지지대가 있고 위 두 임계값 밑인 상태가 이 시간(초) 동안 계속 유지돼야 실제로 Sleep 전환한다.
	// 한 프레임만 느려졌다고 바로 재우면, 다음 프레임 충돌로 또 깨어나는 진동이 그대로 남는다.
	constexpr float SLEEP_DELAY = 0.3f;
	// [접촉만으로 안 깨우기] Sleeping 블록과 부딪힌 쪽의 속도가 이 값(px/s)을 넘어야 "진짜 충격"으로 보고
	// 깨운다. SLEEP_LINEAR_THRESHOLD보다 커야 한다 — 안 그러면 방금 잠든 블록끼리 살짝 스치기만 해도
	// 다시 깨어나는 악순환이 반복된다.
	
	// [고정 timestep] 물리 시뮬레이션 한 스텝의 고정 시간(초). 렌더 FPS와 무관하게 항상 이 크기로만
	// 적분해서, 30/60/144fps에서 같은 장면이 같은 결과를 내게 한다.
	constexpr float PHYSICS_FIXED_TIMESTEP = 1.0f / 60.0f;

	// [고정 timestep] 한 프레임에 최대 이만큼만 물리 스텝을 몰아서 처리한다. 디버거로 멈췄다 풀거나
	// 순간적으로 크게 랙이 걸려도, 밀린 시간을 전부 뒤늦게 다 소화하려 하면("Spiral of Death")
	// 오히려 프레임이 계속 느려지는 악순환에 빠진다 — 그 대신 초과분은 그냥 버린다.
	constexpr int PHYSICS_MAX_SUBSTEPS = 5;
	constexpr float WAKE_IMPACT_SPEED_THRESHOLD = 60.0f;
	// 바닥/다른 블럭에 파고들었다 밀려날 때, 속도를 0으로 죽이는 대신 이 비율만큼만 남기고 반대로 튕겨서
	// 서서히 잦아드는 흔들림(wobble)을 만든다. 1.0에 가까울수록 오래 튕기고, 0에 가까울수록 즉시 멈춤.
	// 블럭은 탱탱볼이 아니라 콘크리트/나무토막에 가까워야 해서 0에 아주 가깝게 잡는다
	constexpr float BOUNCE_RESTITUTION = 0.02f;

	// [위치 보정] 파고든 깊이 중 이 정도(px)는 그냥 무시한다("slop"). 0까지 완벽하게 밀어내려고 하면
	// 미세한 파고듦-보정-재파고듦이 반복되며 계속 떨리는데, 아주 약간의 파고듦은 그냥 봐줘서 그 진동을 끊는다
	constexpr float POSITION_CORRECTION_SLOP = 1.0f;

	// [Sleep 스냅] Sleep()에서 위치를 서브셀 격자에 반올림할 때, 이 값(px) 이내로 이미 격자에 가까울 때만
	// 스냅한다. ANGLE_SNAP_TOLERANCE와 대칭되는 안전장치 — 허용 오차 없이 무조건 반올림하면(최대
	// SUBCELL_SIZE/2 = 12px까지 순간 이동 가능), 물리 솔버가 침투 없이 정착시킨 위치를 다시 밀어넣어
	// 옆 블록과 겹치게 만들 수 있다.
	constexpr float POSITION_SNAP_TOLERANCE = 3.0f;

	// [위치 보정] slop을 뺀 파고든 깊이 중 실제로 밀어낼 비율(0~1). 한 번에 100% 다 밀어내면 그 반동으로
	// 쌓인 블럭들이 스프링처럼 서로 밀어내며 꿀렁거릴 수 있어서, 여러 프레임(솔버 반복)에 걸쳐 나눠서 민다
	constexpr float POSITION_CORRECTION_PERCENT = 0.6f;

	// [위치 보정] 한 번의 충돌 반응이 한 프레임에 밀어낼 수 있는 최대 거리(px). 착지 판정 경계선의
	// 미세한 오차 등으로 아주 드물게 비정상적으로 깊은(수십 px) 파고듦이 생겼을 때, 그걸 한 프레임에
	// 다 밀어내면 순간이동처럼 보인다 — 그 대신 이 한도만큼만 밀어내고 나머지는 다음 프레임들에
	// 걸쳐 서서히 빠져나오게 해서, 어떤 예외적인 상황에서도 순간이동이 나오지 않게 막는 안전장치.
	constexpr float MAX_POSITION_CORRECTION_PER_STEP = 8.0f;

	// [마찰] 접촉면을 따라 미끄러지는 걸 붙잡는 세기. 마찰 임펄스는 이 값 * 수직 방향 임펄스 크기를
	// 못 넘도록 clamp된다(쿨롱 마찰 모델) — 실제 마찰처럼 세게 눌려있을수록 더 세게 붙잡을 수 있다는 뜻.
	// 0이면 마찰 없음(예전 상태), 1에 가까울수록 잘 안 미끄러짐
	constexpr float FRICTION_COEFFICIENT = 0.7f;

	// 무게중심 판정에서, 바닥/다른 블럭 바로 위에 얹혀 있다고 인정할 허용 오차(px)
	constexpr float SUPPORT_CHECK_TOLERANCE = 4.0f;

	// [경사면 미끄러짐 허용] 다른 블럭 위에 "평평한 선반처럼" 안정적으로 얹힐 수 있으려면, 그 아래
	// 블럭이 이 각도(도, 90도 배수 기준 허용 오차) 이내로 축 정렬돼 있어야 한다. GetCellSupportRange의
	// 지지 판정은 회전된 블럭도 그냥 축 정렬 바운딩박스로 뭉개서 "평평한 선반"으로 취급하는 근사라서,
	// 많이 기울어진 블럭(예: 넘어지다 끼어서 비스듬히 멈춘 블럭) 위에 얹힌 것까지 이 근사를 적용하면
	// 실제로는 모서리 하나에 걸쳐 있을 뿐인데도 "안정적으로 지지됨"으로 판정돼 그대로 얼어붙어(Sleep)
	// 버린다 — 경사면이면 미끄러지거나 떨어져야 정상인데 그러지 못하는 원인. 이 각도를 넘는 블럭은
	// 지지 제공자 후보에서 제외해서, 대신 이미 있는 진짜 회전 인식 충돌/마찰 물리(ResolveRigidCollision)로
	// 넘어가게 한다.
	constexpr float SUPPORT_SURFACE_MAX_TILT_DEGREES = 5.0f;

	// [안정 판정 - 안쪽 여유] 무게중심이 지지 범위 "안쪽"으로 이 정도(px)는 들어와 있어야 진짜 안정으로
	// 인정한다. 예전엔 반대 방향(가장자리를 이만큼 넘어야 불안정)으로 썼는데, 그러면 무게중심이 지지
	// 칸의 가장자리에 정확히(0px 초과) 걸친 칼날 위 균형 상태(예: S자를 세로로 세워 한쪽 모서리 하나로
	// 받치는 경우)가 "가장자리를 안 넘었으니 안정"으로 영원히 통과해버렸다 — 실제로는 이런 경우 아주 작은
	// 오차에도 한쪽으로 넘어가야 정상이다. 가장자리에서 이 값만큼 안쪽까지를 "불안정 지대"로 잡아서
	// 해결한다. 값을 넉넉히 벌린 지지 기반(여러 칸)에서는 무게중심이 보통 훨씬 안쪽에 있어서 영향이 없다.
	constexpr float IMBALANCE_DEADZONE = 2.0f;

	// [강제 취침 타임아웃] 지지대가 있는 블럭이 미세한 진동 때문에 속도가 계속 임계값을 살짝 넘나들어
	// TrySleepAll의 정상 판정으로는 영원히 안 잠드는 경우를 위한 안전장치. Awake/Toppling 상태로
	// 이 시간(초)을 넘기면, 지지대가 있는 한 속도와 무관하게 강제로 Sleeping 처리한다.
	constexpr float FORCE_SLEEP_TIMEOUT = 3.0f;

	// [넘어짐 끼임 타임아웃] Toppling 블럭이 옆 블럭이나 바닥 모서리에 끼어서(wedge) 더는 못 넘어가지도,
	// 무게중심이 다시 지지 범위 안으로 돌아와 균형을 되찾지도 못하는 경우를 위한 안전장치.
	// SettleToppledBlocks는 원래 "화면 밖으로 떨어짐" 또는 "균형 회복" 두 경로로만 Toppling을 빠져나가는데,
	// 끼인 블럭은 둘 다 영원히 못 만족해서 무한정 그 상태로 방치된다. 지지대는 있는데 이 시간(초)만큼
	// 계속 Toppling에 머물러 있으면, 실제로 안정된 자세가 아니어도 그 자리에서 강제로 멈춘다(ForceStabilize).
	// FORCE_SLEEP_TIMEOUT과 같은 목적이지만, Toppling 초반엔 진짜로 활발히 넘어지는 시간이 있을 수 있어
	// 별도 상수로 분리해서 독립적으로 조정할 수 있게 한다.
	constexpr float TOPPLE_STUCK_TIMEOUT = 3.0f;

	// [각도 스냅] Sleep()에서 90도 배수와의 차이가 이 값(도) 이내일 때만 각도를 반올림한다.
	// 항상 반올림하면, 다른 블럭에 기대서 진짜로 비스듬히 안정된 블럭까지 강제로 우뚝 세워버리게 된다 —
	// 미세한 물리 오차만 보정하고, 확실히 기울어진 채로 멈춘 건 그대로 둔다
	constexpr float ANGLE_SNAP_TOLERANCE = 12.0f;

	// [바닥 마찰 근사] 지지대가 있는(바닥/다른 블럭 위에 얹힌) Awake/Toppling 블럭에 매 프레임 곱하는
	// 속도 감쇠 비율. ResolveRigidCollision의 임펄스 기반 마찰은 얹혀서 미끄러지는 동안(수직 속도 거의 0)엔
	// 전혀 안 걸리는 구조적 한계가 있어서, 이 감쇠로 "바닥 마찰"을 흉내낸다. 1.0에 가까울수록 안 미끄러지고
	// 멈춤, 0에 가까울수록 잘 미끄러짐
	constexpr float GROUNDED_VELOCITY_DAMPING = 0.75f;

	// [바닥 마찰 근사] 위 GROUNDED_VELOCITY_DAMPING의 각속도 버전. 평평한 바닥에 얹힌 블록은 중력과
	// 바닥 충돌 임펄스가 매 스텝 반복되는데, 각속도는 선속도(DampVelocity)와 달리 이걸로 깎아주지
	// 않으면 미세한 회전이 감쇠 없이 계속 남아 SLEEP_ANGULAR_THRESHOLD를 살짝살짝 넘나들며 영원히
	// Sleep에 못 들어가는 원인이 된다.
	constexpr float GROUNDED_ANGULAR_DAMPING = 0.75f;

	// [넘어짐 피벗 고정] 넘어가기 시작한 직후 이 시간(초) 동안은, 무게중심이 아니라 접촉 모서리를 축으로
	// 고정한 채로 회전시킨다. 회전을 무게중심 기준으로만 하면 바닥에 붙어있던 반대쪽 모서리가 허공으로
	// 붕 뜨는 것처럼 보이는데(실제 도미노는 접촉 모서리가 축이라 안 그럼), 이 시간 동안만 접촉 모서리를
	// 고정해서 그 부자연스러움을 없앤다. 이후엔 자연스럽게 자유낙하로 넘어간다.
	constexpr float TOPPLE_PIVOT_LOCK_DURATION = 0.15f;

	// [무한 재넘어짐 방지] 옆 블록 등에 막혀서 실제로는 못 넘어가는 블록이 Sleep 없이 이만큼 연속으로
	// BeginToppling되면(Awake로 잠깐 돌아왔다가 ResolveBalance가 곧바로 또 넘어뜨리는 걸 반복하면),
	// 물리적으로 완전히 정확한 상태는 아니어도 그 자리에서 강제로 멈춰(ForceStabilize) 무한 진동을 끊는다.
	constexpr int MAX_CONSECUTIVE_TOPPLE_COUNT = 5;

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

	// [카메라] 화면 상단에 이만큼(px) 여유를 남겨두고, 탑이 그 여유를 넘어서 쌓였을 때만 카메라가 따라 올라간다.
	// 값이 작을수록 낮게 쌓여도 금방 카메라가 움직이기 시작한다. 체감에 맞게 조정할 것
	constexpr float CAMERA_TOP_MARGIN = TILE_SIZE * 12.0f;

	// [지지 판정 성능] IsCellSupported가 후보 블록의 실제 모서리(sin/cos 포함)를 계산하기 전에, 두 블록
	// 원점 사이 Y거리로 먼저 거른다. 지금 실제로 스폰되는 블록(TetrominoBlock, 최대 I자 4칸=4타일)
	// 기준으로 블록 원점에서 가장 먼 모서리까지의 거리보다 넉넉히 잡은 값 — 이 거리보다 멀면 절대 안
	// 닿을 수 있으니 검사할 필요가 없다. GiantTetrominoBlock/HeavyBlock에 실제 모양이 생기면(이름상
	// 더 클 수 있음) 이 값도 그에 맞게 다시 검토해야 한다.
	constexpr float SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT = TILE_SIZE * 8.0f;

	// [카메라] 목표 위치(targetY)를 따라가는 속도. 클수록 빨리 따라붙고 작을수록 느긋하게 따라간다.
	// Update()에서 지수 감쇠(1 - e^(-속도*dt))로 쓰여서 프레임레이트가 달라져도 같은 체감 속도를 낸다
	constexpr float CAMERA_FOLLOW_SPEED = 4.0f;

	// [타이틀 화면] 제목(Title.png) 중심의 세로 위치(화면 좌표, px). 가로는 항상 창 가운데로 고정
	constexpr float TITLE_IMAGE_CENTER_Y = WINDOW_HEIGHT * 0.3f;
	constexpr float GAME_OVER_IMAGE_CENTER_Y = WINDOW_HEIGHT * 0.3f;

	// [타이틀 화면] 제목 이미지를 창 안에 맞춘(contain) 크기에 추가로 곱하는 배율. 1.0이면 창에 꽉 차게,
	// 작을수록 더 작게 그린다.
	constexpr float TITLE_IMAGE_SCALE = 0.9f;
	constexpr float GAME_OVER_IMAGE_SCALE = 0.8f;

	// [타이틀 연출] 제목이 낙하를 시작하는 지점의 세로 오프셋(px, 음수=제자리보다 위). 이만큼 위에서
	// 시작해서 떨어지다가 제자리(오프셋 0)에서 튕긴다.
	constexpr float TITLE_DROP_START_OFFSET_Y = -WINDOW_HEIGHT * 0.6f;
	constexpr float GAME_OVER_DROP_START_OFFSET_Y = -WINDOW_HEIGHT * 0.6f;

	// [타이틀 연출] 낙하 가속도(px/s^2). 블록 물리의 GRAVITY와는 무관한 연출 전용 값이라 따로 둔다.
	constexpr float TITLE_DROP_GRAVITY = 2500.0f;
	constexpr float GAME_OVER_DROP_GRAVITY = 2500.0f;

	// [타이틀 연출] 제자리(오프셋 0)에 닿아 튕길 때 남기는 속도 비율(0~1). BOUNCE_RESTITUTION과 같은
	// 개념이지만, 블록은 거의 안 튕겨야 하고 이건 눈에 보이게 살짝 튕겨야 해서 훨씬 크게 잡는다.
	constexpr float TITLE_DROP_BOUNCE_RESTITUTION = 0.35f;
	constexpr float GAME_OVER_DROP_BOUNCE_RESTITUTION = 0.35f;

	// [타이틀 연출] 튕기는 속도가 이 값(px/s) 밑으로 떨어지면 완전히 멈춘 것으로 보고 연출을 끝낸다
	constexpr float TITLE_DROP_SETTLE_SPEED = 40.0f;
	constexpr float GAME_OVER_DROP_SETTLE_SPEED = 40.0f;

	constexpr float GAME_OVER_SCORE_CENTER_Y = WINDOW_HEIGHT * 0.5f;
	constexpr float GAME_OVER_BUTTON_CENTER_Y = WINDOW_HEIGHT * 0.62f;

	// [타이틀 화면] 시작 버튼 중심의 세로 위치(화면 좌표, px). 가로는 항상 창 가운데(WINDOW_WIDTH/2)로 고정.
	// Option/Ranking(비활성)은 이 값 기준으로 버튼 한 칸 높이씩 아래로 이어 붙는다(GetOptionButtonCenter 등 참고)
	constexpr float TITLE_BUTTON_CENTER_Y = WINDOW_HEIGHT * 0.55f;

	// [타이틀 화면] 버튼을 그릴 목표 가로 크기(px). 원본 PNG 비율은 유지한 채 이 너비에 맞춰 축소한다.
	// 원본 이미지가 화면보다 훨씬 커서 그대로 그리면 화면을 뒤덮어버리는 문제 때문에 필요.
	// (5.7에서 5% 줄인 값 — 버튼 전체 크기를 살짝 작게)
	constexpr float TITLE_BUTTON_TARGET_WIDTH = TILE_SIZE * 5.415f;

	// [타이틀 화면] 버튼끼리 세로로 이어 붙일 때, 버튼 높이(GetButtonSize().y)에 더해 추가로 띄울 간격(px)
	constexpr float TITLE_BUTTON_GAP_Y = 14.0f;

	// [타이틀 화면] 클릭 판정 영역은 Button.png에서 알파 채널로 실제 그림 영역만 오려 쓴 BUTTON_SHEET_SOURCE_RECTS
	// 기준이라, 그리는 크기(GetButtonSize())가 이미 버튼 모양에 맞게 측정된 값이다. 그래서 판정 영역도
	// 그리는 크기/중심을 그대로 쓴다 — 별도의 축소 배율이나 위치 보정이 필요 없다.

	// [게임오버 화면] Retry/Title도 TitleScene과 같은 Button.png 시트를 알파 채널로 측정해 쓰는
	// 거라, 클릭 판정도 타이틀처럼 그리는 크기/중심을 그대로 쓴다 — 별도 배율/오프셋 불필요.

	// [타이틀 화면] 아직 기능이 없는 Option/Ranking 버튼을 흐리게(비활성처럼) 그릴 불투명도(0~1)
	constexpr float TITLE_DISABLED_BUTTON_OPACITY = 0.5f;

	//격자선 색상 ( 연한 회색 )
	constexpr COLORREF GRID_LINE_COLOR = RGB(220, 220, 220);
	//블록 타일 색상 (파란색 )
	constexpr COLORREF BLOCK_TILE_COLOR = RGB(30, 100, 240);
	// 디버그용 콜라이더 표시 색상 (빨간색), F1로 토글
	constexpr COLORREF COLLIDER_DEBUG_COLOR = RGB(255, 0, 0);
	// 디버그용 블럭별 물리 상태 텍스트(State/속도/각속도/rest timer) 색상, F1로 토글
	constexpr COLORREF PHYSICS_DEBUG_TEXT_COLOR = RGB(255, 255, 0);

	// 프레임 제한: 목표 FPS와, 그로부터 계산한 한 프레임당 목표 시간(초)
	constexpr int TARGET_FPS = 120;
	constexpr double TARGET_FRAME_SECONDS = 1.0 / TARGET_FPS;

	// [프레임 페이싱] 목표 시간까지 남은 구간이 이 값(초) 이하로 좁혀지면 Sleep()을 더 이상 부르지 않고
	// 스핀(바쁜 대기)으로 전환한다. Sleep()은 OS 스케줄러 지터(보통 0.5~2ms) 때문에 짧은 구간에서
	// 오차가 커서, 마지막 자투리 시간만큼은 CPU를 계속 쓰더라도 스핀으로 정확히 맞추는 편이 낫다.
	constexpr double FRAME_LIMITER_SPIN_MARGIN_SECONDS = 0.002;

	// [FPS 표시] 순간 델타타임으로 매 프레임 fps를 계산하면 위 스핀 마진을 적용해도 남는 미세한
	// 지터가 그대로 숫자에 드러나 깜빡여 보인다. 이 시간(초)만큼 프레임 수를 모았다가 평균을 내서
	// 표시용 FPS를 갱신하면 실제 페이싱은 그대로 두고 화면에 보이는 숫자만 안정된다.
	constexpr float FPS_SMOOTHING_INTERVAL_SECONDS = 0.25f;

	// FPS 표시(디버그용) 위치/색상
	constexpr int FPS_TEXT_MARGIN = 8;
	constexpr COLORREF FPS_TEXT_COLOR = RGB(255, 0, 0);
	
	constexpr float METERS_PER_TILE = 1.0f; // 1타일 = 0.5미터

	// [높이 기록 표시] 화면 위쪽 중앙에 뜨는 "N.Nm" 텍스트의 글자 크기/색, 뒤에 까는 배경 색/불투명도/크기
	constexpr float HEIGHT_RECORD_FONT_SIZE = 32.0f;
	constexpr COLORREF HEIGHT_RECORD_TEXT_COLOR = RGB(255, 255, 255);
	constexpr COLORREF HEIGHT_RECORD_BACKGROUND_COLOR = RGB(0, 0, 0);
	constexpr float HEIGHT_RECORD_BACKGROUND_OPACITY = 0.6f;
	constexpr float HEIGHT_RECORD_PANEL_WIDTH = 200.0f;
	// 배경판 위아래로 글자 크기 기준으로 남길 여백(px)
	constexpr float HEIGHT_RECORD_PANEL_PADDING = 16.0f;

	// [다음 블럭 미리보기] 화면 오른쪽 위 모서리에 Next.png(정사각형 판넬) 배경을 고정 표시하고,
	// 그 위에 다음에 나올 테트로미노를 축소해서 그린다.
	constexpr float NEXT_PANEL_TARGET_WIDTH = TILE_SIZE * 3.0f;
	// 판넬을 화면 모서리에서 얼마나 띄울지(px)
	constexpr float NEXT_PANEL_MARGIN = TILE_SIZE * 0.3f;
	constexpr float NEXT_PANEL_CENTER_X = WINDOW_WIDTH - NEXT_PANEL_TARGET_WIDTH / 2.0f - NEXT_PANEL_MARGIN;
	constexpr float NEXT_PANEL_CENTER_Y = NEXT_PANEL_TARGET_WIDTH / 2.0f + NEXT_PANEL_MARGIN;
	// 판넬 안에 그릴 블럭 칸 하나의 크기(px). 실제 낙하 블럭(TILE_SIZE)보다 작게 그려야 판넬 안에 들어간다.
	// 가장 가로로 긴 모양(I자, 4칸)이 이 값의 4배 폭으로 그려지니, NEXT_PANEL_TARGET_WIDTH보다
	// 너무 커지지 않도록 조정할 것 — 값을 바꿔도 패널 중앙 정렬(RenderNextBlockPreview)은 자동으로 맞춰진다.
	constexpr float NEXT_PANEL_PREVIEW_TILE_SIZE = TILE_SIZE * 0.6f;
	// 블럭 모양을 패널 정중앙보다 얼마나 아래로 내려서 그릴지(px, 양수=아래로)
	constexpr float NEXT_PANEL_PREVIEW_OFFSET_Y = TILE_SIZE * 0.2f;

	// [타이틀 화면 - 옵션 패널] Option 버튼을 누르면 뜨는 볼륨 조절 오버레이. 화면 중앙에 뜬다
	constexpr float OPTION_PANEL_CENTER_Y = WINDOW_HEIGHT * 0.5f;
	// Pop.png(팝업창 배경 그림) 가로 크기를 이 값에 맞추고, 세로는 원본 비율 그대로 계산한다
	// (다른 버튼 이미지들과 같은 방식 — TitleScene::GetOptionPanelSize 참고)
	constexpr float OPTION_PANEL_IMAGE_TARGET_WIDTH = TILE_SIZE * 9.0f;
	// 패널 왼쪽 끝에서 스피커 아이콘까지, 스피커 아이콘에서 첫 볼륨 바까지 남길 여백(px)

	// Ranking.png(랭킹 팝업 그림) 크기를 화면 크기 대비 비율로 직접 정한다(원본 비율은 무시) —
	// 화면을 거의 다 채우도록 가로는 창 너비 그대로, 세로는 창 높이의 80%로 키운다.
	constexpr float RANKING_PANEL_WIDTH_RATIO = 1.0f;
	constexpr float RANKING_PANEL_HEIGHT_RATIO = 0.8f;

	constexpr float OPTION_PANEL_PADDING_X = 28.0f;
	// 볼륨 바들의 공통 바닥선을 패널 아래쪽 끝에서 얼마나 띄울지(px)
	constexpr float OPTION_PANEL_BAR_BOTTOM_MARGIN = 28.0f;

	// [볼륨 바] 칸 5개, 칸당 20%(=1/VOLUME_BAR_COUNT). 칸을 클릭하면 그 칸까지(1~5번째) 볼륨이 채워진다.
	// 각 칸의 실제 크기/간격은 soundbar.png 원본에 그려진 그대로(TitleScene::GetSoundBarElementSize/Center
	// 참고)를 SOUND_BAR_GROUP_SCALE 배율로만 줄이거나 키운다 — 개별 폭/간격을 따로 정하지 않는다.
	constexpr int VOLUME_BAR_COUNT = 5;
	constexpr float VOLUME_BAR_STEP = 1.0f / VOLUME_BAR_COUNT;
	// 현재 볼륨보다 뒤(꺼진) 칸을 흐리게 표시할 불투명도
	constexpr float VOLUME_BAR_DIM_OPACITY = 0.25f;

	// [스피커+볼륨 바 그룹] soundbar.png 안에서 스피커 아이콘과 바 5개가 이루는 배치(크기/간격 비율)를
	// 그대로 유지한 채, 그룹 전체를 이 배율 하나로만 줄이거나 키운다.
	constexpr float SOUND_BAR_GROUP_SCALE = 0.27f;
}
