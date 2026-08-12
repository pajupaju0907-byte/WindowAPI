#pragma once

#include <memory>
#include <string>
#include "../util/Types.h"

class Collider;

// 모든 블럭 종류(테트로미노/원형/거대/무거운 블럭)의 공통 베이스.
// 물리 적분(Integrate)과 힘 적용(ApplyForce)은 블럭 종류와 무관하게 동일하므로 여기서 구현하고,
// 모양/크기 등 데이터만 다른 부분은 파생 클래스의 필드로 처리한다.
//
// 공중 상태(Airborne)와 바닥 상태(Awake/Sleeping)는 서로 다른 이동 방식을 쓴다:
// - 공중 상태: 물리 없이 그리드(서브셀) 좌표를 스텝 이동 (MoveHorizontal/StepDown)
// - 바닥 상태: Land()로 그리드 좌표를 월드 좌표로 확정한 뒤, 물리 적분(ApplyForce/Integrate)으로 이동
class Block
{
public:
	virtual ~Block();

	void AddAngularVelocity(float amount);

	// [바닥 마찰 근사] velocity에 dampingFactor(0~1)를 곱해서 줄인다. ResolveRigidCollision의 임펄스
	// 기반 마찰은 "충돌 순간"에만 계산되고 얹혀서 미끄러지는 중(수직 속도 거의 0)엔 아예 안 걸리는
	// 구조적 한계가 있어서, 지지대가 있는 동안 매 프레임 속도를 깎아 마찰을 흉내낸다
	void DampVelocity(float dampingFactor);

	// [바닥 마찰 근사] 위 DampVelocity의 각속도 버전. 평평한 바닥에 얹힌 블록은 중력->충돌 임펄스가
	// 매 스텝 반복되면서 아주 미세한 회전이 감쇠 없이 계속 남을 수 있는데(선속도와 달리 각속도를
	// 직접 깎는 곳이 없었다), 이게 SLEEP_ANGULAR_THRESHOLD를 살짝살짝 넘나들며 영원히 Sleep에 못
	// 들어가는 원인이었다. 지지대가 있는 동안 매 프레임 각속도도 같이 깎아 회전 마찰을 흉내낸다.
	void DampAngularVelocity(float dampingFactor);
	// originGridX/Y(서브셀 좌표)에 이 블럭이 들어갈 수 있는지 검사한다. 그리드 스냅샷이 아니라
	// 그 순간 실제로 존재하는 착지된 블럭들의 위치(BlockManager)와 바닥 범위(Constants)를 직접 본다 —
	// 그래야 물리로 움직인 블럭 위치와 절대 어긋나지 않는다
	bool CanOccupy(int originGridX, int originGridY) const;

	// 공중 상태 전용: 그리드(서브셀) 기준 이동. 물리 연산 없이 좌표만 스텝 이동한다.
	void MoveHorizontal(int subCellDelta);

	// 한 칸 아래로 이동을 시도한다. CanOccupy로 확인해서
	// 이동했으면 true, 바닥/다른 블럭에 막혀 착지해야 하면 false를 반환한다.
	bool StepDown();

	// StepDown과 같은 조건을 검사하지만 실제로 옮기지는 않는다. 락 딜레이 판정(BlockManager)이
	// "지금 내려갈 수 있는 상태인지"만 매 프레임 확인하고 싶을 때 씀 — 매 프레임 실제로 이동시키면
	// 낙하 속도(FALL_STEP_INTERVAL)와 무관하게 즉시 다 떨어져버리기 때문에 이동 없는 버전이 따로 필요하다.
	bool CanStepDown() const;

	// [그리드-연속 경계] CanStepDown()이 false가 되어 락이 걸린 시점엔, 다음 서브셀로는 못 가지만
	// 그 사이(0 ~ SUBCELL_SIZE px 미만)에 실제 장애물까지 남은 여유가 있을 수 있다. 그 여유를 계산해서
	// Land()가 그리드 칸에 딱 맞춰 배치하는 대신 실제 접촉 위치 가까이에 배치할 수 있게 해준다.
	// 그리드 낙하는 항상 회전 없는(axis-aligned) 상태라 셀별 바닥 Y 비교만으로 정확히 계산 가능하다.
	float ComputeContinuousDropOffset() const;

	//부호로 방향을 받아 모양을 90도 회전시키되, 회전한 모양이 안 들어가면 취소한다
	void Rotate(int direction);
	// 바닥 상태 전용: 착지 이후의 물리 연산(흔들림 등). Awake 상태에서만 호출되어야 한다.
	void ApplyForce(Vector2 force);
	void Integrate(float deltaTime);
	void WakeUp();
	void Sleep();

	// [강제 취침 타임아웃] Awake/Toppling 상태로 얼마나 오래 있었는지 잰다. 지지대가 있는데도
	// 미세한 진동 때문에 속도가 임계값 밑으로 안 떨어져서 영원히 안 잠드는 경우의 안전장치로 쓴다.
	//Land()와 BeginToppling()에서만 리셋되고, Sleep()/WakeUp()에서는 리셋되지 않는다
	void AdvanceActiveTimer(float deltaTime);
	void AdvanceRestTimer(float deltaTime);
	void ResetRestTimer();
	float GetRestTimer() const;
	float GetActiveTimer() const;

	// Airborne -> Awake 전환. 착지 시 BlockManager가 호출해 그리드 좌표를 월드 좌표(m_position)로
	// 확정하고 물리 시뮬레이션을 시작시키는 지점.
	void Land();

	// 스폰 직후 BlockManager가 초기 그리드 좌표를 지정할 때 사용
	void SetGridPosition(int gridX, int gridY);

	// 현재 물리 상태에 맞는 렌더링용 월드 좌표 (Airborne이면 그리드 좌표를 픽셀로 환산, 아니면 m_position)
	Vector2 GetRenderPosition() const;
	int GetCellCount() const;
	Vector2 GetCellRenderPosition(int cellIndex) const;

	// cellIndex번 칸의 "중심"을, 무게중심을 축으로 m_angle만큼 회전시킨 뒤의 월드 좌표로 반환.
	// 물리/충돌 판정에는 안 쓰고 시각 표현(렌더링) 전용 — 스프라이트를 그릴 때는 반드시 이 중심점을
	// 회전 피벗으로 써야 실제 콜라이더(GetCellRotatedCorners)와 어긋나지 않는다
	Vector2 GetCellCenterRotated(int cellIndex) const;

	// localPoint(테트리스 칸 단위로 표현된, 이 블럭에 고정된 한 점 — 예: 셀 모서리)를
	// 무게중심을 축으로 m_angle만큼 돌린 뒤의 월드(픽셀) 좌표로 변환한다.
	// GetCellCenterRotated와 GetCellRotatedCorners가 공통으로 쓰는 회전 공식 본체
	Vector2 RotateLocalPointToWorld(Vector2 localPoint) const;

	// [강체물리 2단계] cellIndex번 칸(사각형 하나)의 네 모서리를 회전 반영한 월드 좌표로 outCorners[0..3]에 채운다.
	// 칸 중심 하나만 보던 GetCellCenterRotated와 달리, "이 사각형이 정확히 어디까지 파고들었는지"를
	// 판정하려면 기울어진 사각형의 네 꼭짓점을 다 알아야 해서 필요하다 (바닥/블럭 충돌에서 씀)
	void GetCellRotatedCorners(int cellIndex, Vector2 outCorners[4]) const;

	// 이 블럭의 무게중심을, m_cellShape와 같은 단위(테트리스 칸 단위)의 "로컬" 좌표로 반환.
	// 4칸이 각각 같은 질량을 가진다고 가정하고 4칸 중심의 평균을 낸다.
	// [강체물리 1단계] 회전은 이제 이 지점을 기준으로 계산한다 (예전엔 90도 스냅 회전용 m_pivot을 대신 썼었음).
	Vector2 GetCenterOfMassLocal() const;

	// 이 블럭의 관성모멘트(회전에 대한 저항, "회전판 질량"에 해당). 단위는 질량*픽셀^2.
	// [강체물리 2단계] ResolveRigidCollision이 "충돌 지점에 가해진 힘이 얼마나 회전을 만들어내는가
	// (각가속도 = 토크 / 관성모멘트)"를 계산할 때 씀
	float GetMomentOfInertia() const;

	// 이 블럭이 차지하는 칸 전체를 감싸는 사각형(월드/픽셀 좌표). 블럭끼리 충돌 판정에 씀
	AABB GetWorldBounds() const;
	const std::string& GetSpriteId() const;

	// Block.png(색깔별 정사각형이 가로로 이어진 시트)에서 이 블럭이 쓸 칸의 인덱스(0부터).
	// 파생 클래스 생성자가 셋팅하며, 렌더링(PlayScene)이 이 값으로 잘라낼 소스 사각형을 계산한다.
	int GetColorSlotIndex() const;

	// 질량 상관없이 같은 중력 가속도를 주려면, 힘을 걸 때 질량을 곱해줘야 해서 필요 (F=ma에서 a를 고정하려는 것)
	float GetMass() const;

	// 속도의 제곱크기(sqrt 안 써서 가볍게). 안정성 판정에서 임계값이랑 비교할 때 씀
	float GetSpeedSquared() const;

	// 렌더링용 회전 각도(도 단위)
	float GetAngle() const;
	// 각속도(도/초). CheckGlobalStability가 회전까지 안정적인지 판단할 때 씀
	float GetAngularVelocity() const;

	// PhysicsManager가 Awake 상태 블럭만 골라내려고 씀
	PhysicsState GetPhysicsState() const;


	// 아래로 파고든 만큼(penetration) 위로 밀어내고 낙하를 멈춘다. 지금은 ResolveBlockPairCollision(블럭끼리
	// 충돌, 3단계에서 회전 반영 예정)에서만 쓰고, 바닥 충돌은 ResolveRigidCollision으로 대체되었다
	void ResolveVerticalPenetration(float penetration);

	// [강체물리 2단계] 회전을 반영한 진짜 강체 충돌 처리. contactPoint(월드 좌표)는 실제로 부딪힌 지점,
	// normal은 그 지점에서 밀어내야 할 방향(바닥이면 항상 위 방향 (0,-1)), penetration은 파고든 깊이(px).
	// 무게중심이 아니라 contactPoint에서 부딪혔기 때문에, 밀려나는 힘의 일부가 회전(각속도)으로 새어나간다 —
	// 그게 실제 물체가 한쪽 모서리로 떨어지면 팽그르르 도는 이유다
	void ResolveRigidCollision(Vector2 contactPoint, Vector2 normal, float penetration);

	// [강체물리 3단계] ResolveRigidCollision의 "상대는 안 움직이는 바닥" 버전과 달리, other도 실제로
	// 움직이고 회전하는 강체일 때 쓴다. normal은 "other에서 this를 향하는 방향"으로 약속한다.
	// 뉴턴의 3법칙대로 this와 other에 정확히 반대 방향의 임펄스를 나눠 적용한다
	void ResolveRigidCollisionWithBlock(Block* other, Vector2 contactPoint, Vector2 normal, float penetration);

	// [무게중심 보조 토크] imbalance: 무게중심이 지지 범위를 벗어난 정도(px, 부호로 방향 표시).
	// 순수 물리(중력+접촉점 충돌)만으로는 실제로 넘어뜨리기엔 너무 약하고 느려서, 벗어난 만큼 아주 작은
	// 각가속도를 보조로 "더해준다"(덮어쓰지 않음) — 실제 회전은 여전히 충돌 임펄스가 담당하고, 이건 그게
	// 시작되도록 살짝 떠미는 역할일 뿐이다. 균형 범위 안이면 아무것도 안 한다 — 감쇠는 마찰/충돌에 맡긴다
	void ApplyBalanceTorque(float imbalance, float deltaTime);

	// Awake -> Toppling 전환. 그 순간의 실제 속도/각속도를 그대로 이어받아 계속 낙하한다.
	// PhysicsManager::Update가 매 프레임 GetAngle()이 MAX_TOPPLE_ANGLE을 넘었는지 직접 감시하다가 호출한다 —
	// 각속도가 보조 토크에서 왔든 실제 충돌에서 왔든 상관없이 이 각도를 넘으면 무조건 여기서 걸리므로,
	// 무한 회전 걱정 없이 ApplyBalanceTorque를 마음 놓고 쓸 수 있다
	void BeginToppling();

	// [넘어짐 피벗 고정] pivotWorld(월드 좌표)를 축으로 삼아, TOPPLE_PIVOT_LOCK_DURATION 동안 그 지점이
	// 화면에서 안 움직이도록 위치를 고정한 채 회전만 시킨다 — 무게중심 축 회전 때문에 접촉 모서리가
	// 허공으로 붕 뜨는 걸 막는 용도. BeginToppling() 직후에 호출해서 쓴다.
	void SetTopplePivot(Vector2 pivotWorld);

protected:
	// unique_ptr<Collider>가 불완전 타입을 가리키므로, 기본 생성자는 반드시 .cpp(Collider가 완전한 타입인 곳)에서 정의한다.
	Block();

	int m_id = 0;

	// 블럭을 구성하는 칸들의 원점 기준 상대 좌표 (테트리스 칸 단위, 예: O자는 (0,0)(1,0)(0,1)(1,1)).
	// 이동/충돌(StepDown, MoveHorizontal)과 렌더링이 여기를 순회하므로, 파생 클래스 생성자에서 반드시 채워야 한다.
	static constexpr int CELL_COUNT = 4;
	Vector2 m_cellShape[CELL_COUNT];
	Vector2 m_pivot;
	bool m_canRotate = true;
	// 공중 상태(Airborne)에서 쓰는 그리드 좌표. 단위는 서브셀(1 서브셀 = 0.5 테트리스 칸).
	int m_gridX = 0;
	int m_gridY = 0;

	// 바닥 상태(Land() 호출 이후)에서 쓰는 월드 좌표/속도. 그 전에는 의미 없는 값이다.
	Vector2 m_position;
	Vector2 m_velocity;
	Vector2 m_accumulatedForce;
	float m_angle = 0.0f;
	float m_angularVelocity = 0.0f;
	float m_mass = 1.0f;
	float m_activeTimer = 0.0f;

	// [성능] RotateLocalPointToWorld가 매 호출마다 sin/cos를 다시 계산하지 않도록 캐싱한다.
	// 물리 루프(지지 판정/충돌)에서 같은 블럭에 대해 한 프레임에도 수십~수백 번 불릴 수 있고,
	// 특히 Sleeping 블럭은 m_angle이 그대로라 매번 재계산하는 게 완전히 낭비였다.
	// const 메서드 안에서 갱신해야 해서 mutable로 둔다.
	mutable float m_cachedTrigAngle = 0.0f;
	mutable float m_cachedCosAngle = 1.0f;
	mutable float m_cachedSinAngle = 0.0f;
	mutable bool m_trigCacheValid = false;

	// [넘어짐 피벗 고정] SetTopplePivot()이 설정하는 상태. m_pivotLockTimeRemaining이 0보다 큰 동안,
	// Integrate()가 위치를 그냥 적분하는 대신 m_pivotWorldTarget이 고정되도록 역산해서 덮어쓴다.
	Vector2 m_pivotWorldTarget = { 0.0f, 0.0f };
	Vector2 m_pivotOffsetAtCapture = { 0.0f, 0.0f };
	float m_pivotCaptureAngle = 0.0f;
	float m_pivotLockTimeRemaining = 0.0f;
	// [블록별 rest timer] 지지대가 있고 속도가 SLEEP 임계값 밑인 상태가 얼마나 지속됐는지 잰다.
	// m_activeTimer(강제취침 안전장치)와 달리 이건 "진짜로 Sleep해도 되는가"를 판단하는 정상 경로다.
	// 조건이 깨지면(지지 소실/충돌/속도 초과) 즉시 0으로 리셋된다.
	float m_restTimer = 0.0f;
	// [그리드 스냅] 한 번이라도 Toppling을 거쳤으면 true. 정상 착지해서 흔들리다 멈춘 블럭은 진짜
	// 테트리스처럼 격자에 딱 맞게 스냅시키고 싶지만, 넘어진 블럭까지 스냅하면 물리로 자연스럽게 기운
	// 각도가 강제로 90도 단위로 꺾여버려 어색해진다 — 그래서 Toppling을 거친 적 있는지 구분해서 쓴다
	bool m_hasToppled = false;
	PhysicsState m_physicsState = PhysicsState::Airborne;
	std::unique_ptr<Collider> m_collider;
	std::string m_spriteId;
	int m_colorSlotIndex = 0;
};
