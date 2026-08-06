#pragma once

#include "../util/Constants.h"
#include "../util/Types.h"
#include <vector>

class Block;

// [강체물리 3단계] 두 칸(사각형) 사이의 SAT 충돌 검사 결과.
// normal은 항상 "TestCellCollision(cornersA, cornersB) 호출 기준으로 B에서 A를 향하는 방향"으로 통일한다 —
// Block::ResolveRigidCollision(WithBlock)이 전부 이 방향 규약("other -> this")을 전제로 짜여 있기 때문.
//
// [접촉 다각화] 접촉점을 1개만 쓰면, 면과 면이 맞닿는 상황(블럭이 평평하게 얹힌 경우)에서도 마치
// 뾰족한 점 하나로 균형을 잡고 있는 것처럼 계산되어 계속 미세하게 회전하며 불안정해진다.
// 그래서 B의 꼭짓점 중 겹침이 깊은 2개(contactPoints[0], [1])를 같이 돌려준다 — 코너 충돌처럼
// 진짜 접촉점이 1개뿐인 경우엔 두 점이 같은 자리를 가리키게 되어 자연히 1점 충돌과 동일하게 동작한다.
struct CellCollisionResult
{
    bool collided = false;
    Vector2 normal = { 0.0f, 0.0f };
    float penetration = 0.0f;
    Vector2 contactPoints[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
};

// 바닥 상태(Awake)인 블럭들의 물리 갱신과 전체 안정성 판단을 담당하는 싱글톤.
// 공중 상태(Airborne) 블럭의 낙하는 그리드 스텝 방식이라 BlockManager가 처리하며, 여기서는 다루지 않는다.
class PhysicsManager
{
public:
    static PhysicsManager& GetInstance();

    // BlockManager가 가진 블럭 전체를 순회하며 Awake 상태만 골라 ApplyGravity + Integrate 호출
    void Update(float deltaTime);

    // 바닥 상태에서의 흔들림 표현을 위한 중력 적용. 공중 낙하 속도와는 무관하다.
    void ApplyGravity(Block* block);

    // block이 차지하는 칸들 중 바닥을 가장 깊이 파고든 정도를 찾아서, 파고들었으면 밀어냄
    void ResolveFloorCollision(Block* block);

    // [강체물리 3단계] block과 other가 차지하는 칸(최대 4x4=16쌍)을 전부 SAT로 검사해서, 겹친 쌍마다
    // 강체 충돌 반응을 적용한다. 둘 다 Awake면 쌍방향(ResolveRigidCollisionWithBlock), 한쪽만 Awake면
    // Sleeping인 쪽을 바닥처럼 고정된 것으로 취급(ResolveRigidCollision)한다
    void ResolveBlockPairCollision(Block* block, Block* other);
    // 존재하는 모든 블럭 쌍(중복 없이 한 번씩)에 대해 ResolveBlockPairCollision을 돌림
    void ResolveBlockCollisions();

    // [강체물리 3단계] cornersA/cornersB(각각 GetCellRotatedCorners로 얻은, 회전 반영된 사각형 네 꼭짓점)
    // 사이를 SAT(분리축 정리)로 검사한다. 겹쳐 있으면 방향/깊이/접촉점 2개까지 계산해서 돌려준다
    CellCollisionResult TestCellCollision(const Vector2 cornersA[4], const Vector2 cornersB[4]) const;
    // corners를 axis(단위벡터) 위에 투영했을 때의 최소/최대값을 구하는 SAT 보조 함수
    void ProjectCornersOntoAxis(const Vector2 corners[4], Vector2 axis, float& outMin, float& outMax) const;
    // corners 네 점의 중심(단순 평균) — 정사각형이라 이게 곧 기하학적 중심이다
    Vector2 ComputeQuadCenter(const Vector2 corners[4]) const;

    // cellPosition(월드 좌표, TILE_SIZE 정사각형 기준)이 바닥이나 self가 아닌 다른 블럭 위에 얹혀 있는지 판정
    bool IsCellSupported(Vector2 cellPosition, Block* self) const;
    // [무게중심 보조 토크] block의 무게중심(x)이 지지된 칸들의 가로 범위를 벗어났으면
    // Block::ApplyBalanceTorque로 작은 보조 각가속도를 건다. 실제 회전은 여전히 충돌 임펄스가 담당하고
    // 이건 그게 시작되도록 살짝 떠미는 역할일 뿐이라, 균형 잡힌 경우엔 아무것도 안 한다(감쇠 없음)
    void ResolveBalance(Block* block, float deltaTime);

    // Toppling 상태인 블럭 중 화면 아래로 완전히 벗어난 것들을 BlockManager에서 제거
    void RemoveToppledBlocks();

    // Toppling 상태인 블럭 중 화면 밖으로 떨어지지 않고 바닥/다른 블럭 위에서 실제로 멈춘 것들을
    // Sleeping으로 전환한다. 이게 없으면 착지해서 멈춘 것처럼 보여도 계속 Toppling 상태로 남아
    // 매 프레임 중력+충돌 보정을 영원히 받아서, 미세하게 계속 미끄러지고 튕기는 것처럼 보인다
    void SettleToppledBlocks();

    // [강제 취침 타임아웃] Awake/Toppling 상태가 FORCE_SLEEP_TIMEOUT을 넘긴 블럭 중, 지지대가 있는
    // 것들을 속도와 무관하게 강제로 Sleeping 처리한다. SettleToppledBlocks/TrySleepAll의 속도 기반
    // 판정이 미세한 진동 때문에 계속 통과하지 못하는 경우를 위한 최종 안전장치
    void ForceSleepStuckBlocks();

    // 개별 블럭이 아니라 탑 전체가 안정적인지 판단 (전체 판단은 매니저의 책임)
    bool CheckGlobalStability() const;

    void TrySleepAll();
	void WakeAll();

    // [디버그 시각화] block 자신의 지지 범위(outMinX/outMaxX)와, block이 떠받치는 전체 덩어리의 결합
    // 무게중심(outCombinedComX)을 계산해서 그대로 돌려준다 — ResolveBalance와 완전히 같은 계산이라,
    // F1 디버그 오버레이가 "지금 이 블럭이 안정적이라고 판단된 이유"를 화면에 그대로 그릴 수 있게 해준다.
    // 지지대가 없으면 false를 반환(이 경우 outMinX/outMaxX/outCombinedComX는 의미 없음)
    bool ComputeSupportDebugInfo(Block* block, float& outMinX, float& outMaxX, float& outCombinedComX) const;
private:
    PhysicsManager() = default;
    ~PhysicsManager();
    PhysicsManager(const PhysicsManager&) = delete;
    PhysicsManager& operator=(const PhysicsManager&) = delete;

    // upper의 칸 중 하나라도 lower 위에 얹혀 있는지(IsCellSupported와 같은 판정을, 특정 블록 한 쌍으로 좁혀서) 검사
    bool RestsOnBlock(Block* upper, Block* lower) const;

    // [연쇄 붕괴] base 위에 (직접 또는 다른 블럭을 거쳐 간접적으로) 얹힌 모든 블럭을 재귀로 훑어서,
    // base를 포함한 전체의 질량 합과 무게중심(x) 가중합을 누적한다. ResolveBalance가 "이 블럭 자신"이 아니라
    // "이 블럭이 떠받치고 있는 전체 무더기"의 무게중심으로 판정하게 하려고 필요하다 —
    // 안 그러면 위에 삐딱하게 얹힌 블럭만 넘어지고 밑을 받치는 블럭은 자기 발판만 보고 멀쩡하다고 착각한다.
    void AccumulateSupportedMass(Block* base, std::vector<Block*>& visited, float& outTotalMass, float& outWeightedX) const;
};
