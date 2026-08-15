#pragma once

#include <memory>
#include <string>
#include "../util/Types.h"

class Collider;
class OBBCollider;

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

	// [충돌 판정 책임 분리] cellIndex번 칸을 OBBCollider로 표현해서 돌려준다. 회전 반영된 칸 중심
	// (GetCellCenterRotated)과 칸 크기의 절반(halfExtents), 블럭의 현재 각도를 그대로 담는다 —
	// GetCellRotatedCorners와 수학적으로 동일한 네 꼭짓점을 만들어내며, CollisionManager가 칸 단위
	// SAT 검사를 할 때 이 콜라이더를 통해서만 접근하게 해서 충돌 판정 로직을 한 곳에 모은다.
	OBBCollider GetCellCollider(int cellIndex) const;

	// 이 블럭의 무게중심을, m_cellShape와 같은 단위(테트리스 칸 단위)의 "로컬" 좌표로 반환.
	// 칸별 질량은 m_cellWeight 비율을 따른다(기본은 전부 동일 → 4칸 중심의 단순 평균과 동일).
	// [강체물리 1단계] 회전은 이제 이 지점을 기준으로 계산한다 (예전엔 90도 스냅 회전용 m_pivot을 대신 썼었음).
	Vector2 GetCenterOfMassLocal() const;

	// 이 블럭의 관성모멘트(회전에 대한 저항, "회전판 질량"에 해당). 단위는 질량*픽셀^2.
	// [강체물리 2단계] ResolveRigidCollision이 "충돌 지점에 가해진 힘이 얼마나 회전을 만들어내는가
	// (각가속도 = 토크 / 관성모멘트)"를 계산할 때 씀. 칸별 질량은 m_cellWeight 비율을 따른다.
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


	// [강체물리 2단계] 회전을 반영한 진짜 강체 충돌 처리. contactPoint(월드 좌표)는 실제로 부딪힌 지점,
	// normal은 그 지점에서 밀어내야 할 방향(바닥이면 항상 위 방향 (0,-1)), penetration은 파고든 깊이(px).
	// 무게중심이 아니라 contactPoint에서 부딪혔기 때문에, 밀려나는 힘의 일부가 회전(각속도)으로 새어나간다 —
	// 그게 실제 물체가 한쪽 모서리로 떨어지면 팽그르르 도는 이유다. 호출부(PhysicsManager)가 접촉점 2개에
	// 대해 이 함수를 두 번 불러서(첫 번째는 penetration 그대로, 두 번째는 0으로) 접촉면을 다각화한다.
	// [한때 두 점을 한 번에 받아 "동시 처리"하도록 바꿨다가 되돌렸다] 얹혀서 쉬는 상태의 미세한 회전
	// 편향(첫 점은 각속도 0으로 평가돼 마찰이 안 걸리고, 둘째 점은 그 각속도 때문에 생긴 가짜 미끄러짐이
	// 마찰로 잡혀서 비대칭해짐)을 고치려던 시도였는데, 진짜 충돌(속도가 큰 경우)에서는 두 점의 독립적인
	// 보정을 그냥 더하는 게 실제 필요량보다 훨씬 큰 임펄스를 만들어서 블록이 통째로 날아가는 훨씬
	// 심각한 폭주로 이어졌다(physics_debug.log로 실전 확인 — 한 번의 충돌 처리에서 각속도가 수백 도/초대로
	// 튐). 검증된 순차 처리(+ Step()의 4회 반복 수렴)로 되돌린다.
	void ResolveRigidCollision(Vector2 contactPoint, Vector2 normal, float penetration);

	// [강체물리 3단계] ResolveRigidCollision의 "상대는 안 움직이는 바닥" 버전과 달리, other도 실제로
	// 움직이고 회전하는 강체일 때 쓴다. normal은 "other에서 this를 향하는 방향"으로 약속한다.
	// 뉴턴의 3법칙대로 this와 other에 정확히 반대 방향의 임펄스를 나눠 적용한다.
	void ResolveRigidCollisionWithBlock(Block* other, Vector2 contactPoint, Vector2 normal, float penetration);

	// [무게중심 실제 토크] pivotWorld(월드 좌표)를 축으로 삼아, 중력이 만들어내는 실제 회전력(토크)을
	// 매 프레임 각속도에 조금씩 더한다. 무게중심이 pivot에서 멀수록(더 많이 기울수록) 지렛대가 길어져서
	// 더 세게 가속되고, 관성모멘트가 큰(길쭉하거나 넓게 퍼진) 모양일수록 덜 가속된다 — 실제 도미노가
	// 처음엔 천천히, 기울수록 빨리 넘어가는 이유와 같은 원리. 순간적으로 각속도를 꽂아넣던 예전 방식
	// (TUMBLE_ANGULAR_VELOCITY 킥) 대신 진짜 중력 물리로 대체한 것. 균형 범위 안이면 애초에 호출되지 않는다
	void ApplyGravityTorque(Vector2 pivotWorld, float deltaTime);

	// Awake -> Toppling 전환. 그 순간의 실제 속도/각속도를 그대로 이어받아 계속 낙하한다.
	// PhysicsManager::Update가 매 프레임 GetAngle()이 MAX_TOPPLE_ANGLE을 넘었는지 직접 감시하다가 호출한다 —
	// 각속도가 중력 토크에서 왔든 실제 충돌에서 왔든 상관없이 이 각도를 넘으면 무조건 여기서 걸리므로,
	// 무한 회전 걱정 없이 ApplyGravityTorque를 마음 놓고 쓸 수 있다
	void BeginToppling();

	// [무한 재넘어짐 방지] BeginToppling()이 Sleep() 없이 연속으로 몇 번 호출됐는지 돌려준다.
	// PhysicsManager::ResolveBalance가 이 값이 MAX_CONSECUTIVE_TOPPLE_COUNT를 넘으면 다시 넘어뜨리는
	// 대신 ForceStabilize()로 강제로 멈추게 하는 데 쓴다.
	int GetConsecutiveToppleCount() const;

	// [넘어짐 재발 판정 기준각 — 실전에서 발견된 무한 진동 버그 수정] 이 블록이 마지막으로 "확실히
	// 안정됐다"고 확인된 순간의 각도. Land()에서 0으로 리셋되고, Sleep()이나(TrySleepAll/ForceStabilize
	// 경유 포함) SettleToppledBlocks가 Toppling 상태에서 진짜로 균형을 되찾아 Awake로 돌려보낼 때마다
	// 그 순간의 각도로 갱신된다. MAX_TOPPLE_ANGLE 비교는 절대각(GetAngle())이 아니라 반드시 "이 기준각에서
	// 얼마나 더 돌았는지"로 해야 한다 — 안 그러면 90/135도처럼 완전히 넘어져서 옆으로 누운 채 정상적으로
	// 안정된 블록도, 절대각이 여전히 40도를 넘는다는 이유만으로 Step()의 안전장치가 즉시 다시
	// BeginToppling()을 불러버리고, SettleToppledBlocks는 다시 균형을 확인해 Awake로 돌려보내는 걸
	// 매 프레임 무한 반복하며 제자리에서 부들부들 떠는 버그가 생긴다(physics_debug.log로 실전 확인 —
	// Toppling↔Awake가 매 스텝 번갈아 찍힘).
	void ConfirmRestingAngle();
	float GetRestAngleReference() const;

	// [진짜 넘어지는 중 보호] ResolveBalance가 이번 스텝에 무게중심 불균형을 감지해서 실제 중력 토크를
	// 걸고 있는 동안 계속 누적되는 시간(초). 균형이 돌아오면(또는 Land()/BeginToppling()) 0으로 리셋된다.
	// PhysicsManager::TrySleepAll이 이 값이 0보다 크면 각속도 임계값 기준으로 재우거나 강제 정지시키지
	// 않는다 — 안 그러면 넘어지는 도중의 정상적인 회전 가속을 "떨림"으로 착각해서 매번 끊어버려서,
	// 명백히 넘어져야 할 블록이 조금씩만 진행하다 번번이 멈추고 끝내 못 넘어가는 문제가 생긴다(실전 확인됨).
	void AdvanceImbalanceTimer(float deltaTime);
	// 균형이 돌아왔을 때 호출 — 불균형 지속시간과 아래 "끼임" 판정을 함께 초기화한다.
	void ResetImbalanceTimer();
	float GetImbalanceTimer() const;

	// [pivot 고정 — 넘어짐 방향 오류 방지] 이번 불균형 에피소드의 토크 계산용 pivot을 고정해서 기억해둔다.
	// 매 프레임 그 순간의 회전된 발판에서 pivot을 다시 뽑으면, 회전이 진행될수록 무게중심에서 먼 발판이
	// 무게중심을 축으로 옆으로 휩쓸리면서 pivot 선택 자체가 뒤집힐 수 있다 — 우연한 미세한 초기 흔들림
	// 방향이 스스로를 강화하는 피드백 루프가 되어, 반대쪽으로 넘어가야 할 블록이 그 흔들림 방향 그대로
	// 가속돼버린다(세로로 긴 I자 블록에서 실전 확인됨). PhysicsManager::ResolveBalance가 에피소드 시작
	// 프레임에만 새로 계산해서 여기 저장하고, 이후로는 각도가 얼마나 돌든 이 값을 그대로 재사용한다.
	void SetImbalancePivot(Vector2 pivot);
	Vector2 GetImbalancePivot() const;

	// [무게 전달 계산 전용 — 실전에서 발견된 "위쪽 무게가 아래에 안 전달됨" 버그 수정, 콜리전과 안 싸우는
	// 버전] 무게중심은 순수 회전만으로는 위치가 안 바뀌기 때문에, 기울어지는 중인 블록의 진짜 m_position은
	// 거의 그대로다 — 그래서 AccumulateSupportedMass(위/아래 블록의 결합 무게중심 계산)가 실제 위치만
	// 보면, 아무리 기울어도 그 위에/아래에 있는 블록들이 전혀 눈치를 못 챈다. 예전엔 이걸 고치려고
	// m_position 자체를 pivot 축으로 강제로 swing시켰는데, 그러면 콜리전 해결이 매 프레임 밀어낸 위치
	// 보정을 다음 프레임 이 강제 위치 계산이 통째로 덮어써버려서, 콜리전과 서로 되돌리기를 반복하며
	// 각속도만 폭주하는 훨씬 심각한 버그가 생겼다(실전 확인됨). 그래서 이번엔 m_position/충돌/렌더링은
	// 전혀 안 건드리고, "이 pivot을 축으로 지금 각도까지 진짜로 swing했다면 무게중심이 어디 있을지"를
	// 계산만 해서 돌려주는 순수 조회 함수로 만들었다 — AccumulateSupportedMass가 무게 합산에 쓰는 X
	// 좌표만 이걸로 바꾸면, 실제 위치/충돌에는 부작용 없이 위쪽이 기울수록 아래가 그 쏠림을 느끼게 된다.
	// 불균형 에피소드 중이 아니면(GetImbalanceTimer()<=0) 그냥 평범한 실제 무게중심을 돌려준다.
	Vector2 GetEffectiveWeightPositionWorld() const;

	// [끼임 판정] ForceStabilize()가 호출될 때마다(어느 이유에서든) 같이 켜지는 "포기" 표시. 켜져 있는
	// 동안은 ResolveBalance가 같은(안 풀린) 불균형을 다시 재시도하지 않는다 — 안 그러면 강제 정지 직후
	// 똑같은 불균형이 또 감지돼 곧바로 다시 깨우는 무한 루프가 된다(실전 확인됨). 균형이 돌아오면
	// ResetImbalanceTimer()에서 같이 풀린다. 콜리전 등 다른 경로로 WakeUp되는 것과는 무관하다 —
	// 그땐 다음 프레임에 지지 상황이 새로 평가된다.
	bool IsWedged() const;

	// [무한 재넘어짐 방지] 재넘어짐 카운트를 0으로 되돌린다. Sleep() 자체는 이걸 하지 않는다(ForceStabilize도
	// Sleep()을 거치기 때문 — 강제로 재운 것까지 리셋하면 다음 프레임에 또 5번 재시도하는 묶음이 반복된다).
	// PhysicsManager::TrySleepAll처럼 "진짜로 안정돼서 스스로 잠들었다"고 확인된 경로에서만 호출한다.
	void ResetConsecutiveToppleCount();

	// [무한 재넘어짐 방지] 옆 블록 등에 막혀서 실제로는 못 넘어가는데 계속 넘어지려고 진동하는 블록을
	// 강제로 멈춘다. 속도/각속도를 0으로 죽이고 곧바로 Sleep() 시킨다 — 물리적으로 "정확히" 안정된 상태가
	// 아니어도, 무한 진동보다는 그 자리에서 멈추는 쪽이 낫다는 판단.
	void ForceStabilize();

protected:
	// unique_ptr<Collider>가 불완전 타입을 가리키므로, 기본 생성자는 반드시 .cpp(Collider가 완전한 타입인 곳)에서 정의한다.
	Block();

	int m_id = 0;

	// 블럭을 구성하는 칸들의 원점 기준 상대 좌표 (테트리스 칸 단위, 예: O자는 (0,0)(1,0)(0,1)(1,1)).
	// 이동/충돌(StepDown, MoveHorizontal)과 렌더링이 여기를 순회하므로, 파생 클래스 생성자에서 반드시 채워야 한다.
	static constexpr int CELL_COUNT = 4;
	Vector2 m_cellShape[CELL_COUNT];
	// 칸별 상대 무게 배율(절대 질량이 아니라 비율). 기본은 전부 1.0이라 모든 칸이 동일 질량인
	// 기존 동작과 완전히 같다. 특정 칸을 더 무겁게 만들고 싶은 파생 클래스가 생성자에서 이 배열만
	// 채우면 되고, 총합 대비 비율로 정규화하는 건 GetCenterOfMassLocal()/GetMomentOfInertia()가
	// 알아서 처리하므로 합이 1일 필요는 없다 (예: {1, 1, 1, 3}처럼 그냥 상대 크기만 맞추면 됨).
	float m_cellWeight[CELL_COUNT] = { 1.0f, 1.0f, 1.0f, 1.0f };
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

	// [넘어짐 재발 판정 기준각] 마지막으로 안정이 확인된 각도. Land()에서 0으로, ConfirmRestingAngle()
	// 호출마다 그 순간 각도로 갱신된다.
	float m_restAngleReference = 0.0f;
	// [블록별 rest timer] 지지대가 있고 속도가 SLEEP 임계값 밑인 상태가 얼마나 지속됐는지 잰다.
	// m_activeTimer(강제취침 안전장치)와 달리 이건 "진짜로 Sleep해도 되는가"를 판단하는 정상 경로다.
	// 조건이 깨지면(지지 소실/충돌/속도 초과) 즉시 0으로 리셋된다.
	float m_restTimer = 0.0f;
	// [그리드 스냅] 한 번이라도 Toppling을 거쳤으면 true. 정상 착지해서 흔들리다 멈춘 블럭은 진짜
	// 테트리스처럼 격자에 딱 맞게 스냅시키고 싶지만, 넘어진 블럭까지 스냅하면 물리로 자연스럽게 기운
	// 각도가 강제로 90도 단위로 꺾여버려 어색해진다 — 그래서 Toppling을 거친 적 있는지 구분해서 쓴다
	bool m_hasToppled = false;
	// [무한 재넘어짐 방지] BeginToppling()이 Sleep() 없이 연속으로 몇 번 불렸는지 센다. 옆 블록에 막혀서
	// 실제로는 못 넘어가는데 ResolveBalance가 무게중심 판정만 보고 매번 다시 넘어뜨리려 드는 경우, 지지대가
	// 있고 속도가 낮아져도(SettleToppledBlocks) Awake로 돌아오자마자 곧바로 또 Toppling으로 되돌아가길
	// 반복한다. Sleep()에서 0으로 리셋되고, BeginToppling()마다 1씩 늘어난다.
	int m_consecutiveToppleCount = 0;
	// [진짜 넘어지는 중 보호 / 끼임 판정] ResolveBalance가 무게중심 불균형을 감지해서 중력 토크를 거는
	// 동안 끊기지 않고 누적되는 시간. 균형이 돌아오거나 Land()/BeginToppling() 때 0으로 리셋된다.
	float m_imbalanceTimer = 0.0f;
	// 위 타이머가 TOPPLE_STUCK_TIMEOUT을 넘도록 못 넘어가면 켜진다 — 같은 불균형을 다시 재시도하지
	// 않기 위한 표시. ResetImbalanceTimer()에서 같이 풀린다.
	bool m_isWedged = false;
	// [pivot 고정] 이번 불균형 에피소드에서 계속 재사용할 토크 계산용 pivot(월드 좌표).
	Vector2 m_imbalancePivot = { 0.0f, 0.0f };
	// [무게 전달 계산 전용] 이번 불균형 에피소드가 시작된 순간의 실제 무게중심/각도. SetImbalancePivot()
	// 호출 시점에 같이 캡처되고, GetEffectiveWeightPositionWorld()가 "그 이후로 pivot을 축으로 얼마나
	// 더 돌았는지"를 계산하는 기준으로만 쓴다 — m_position 자체는 절대 여기서 갱신하지 않는다.
	Vector2 m_imbalanceCenterOfMassAtStart = { 0.0f, 0.0f };
	float m_imbalanceAngleAtStart = 0.0f;
	PhysicsState m_physicsState = PhysicsState::Airborne;
	std::unique_ptr<Collider> m_collider;
	std::string m_spriteId;
	int m_colorSlotIndex = 0;
};
