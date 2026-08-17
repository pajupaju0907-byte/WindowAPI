#pragma once

#include "../util/Constants.h"
#include "../util/Types.h"
#include <vector>
#include <unordered_map>
#include <utility>

class Block;

// 바닥 상태(Awake)인 블럭들의 물리 갱신과 전체 안정성 판단을 담당하는 싱글톤.
// 공중 상태(Airborne) 블럭의 낙하는 그리드 스텝 방식이라 BlockManager가 처리하며, 여기서는 다루지 않는다.
class PhysicsManager
{
public:
    // [무게 분배] base -> 그 위에 얹혀 무게를 넘기는 자식들의 목록. 자식 하나가 서로 다른 여러 base
    // 위에 걸쳐 있을 수 있어서(예: L자가 두 블록 위에 한 칸씩), 자식마다 "이 base가 그 자식 무게의
    // 몇 %를 부담하는지"(0~1, 그 자식의 모든 base에 걸친 비율 합은 1)를 같이 들고 있다.
    // BuildRestingChildrenMap 주석 참고.
    using RestingChildrenMap = std::unordered_map<Block*, std::vector<std::pair<Block*, float>>>;

    static PhysicsManager& GetInstance();

    // BlockManager가 가진 블럭 전체를 순회하며 Awake 상태만 골라 ApplyGravity + Integrate 호출
    void Update(float deltaTime);

    // 바닥 상태에서의 흔들림 표현을 위한 중력 적용. 공중 낙하 속도와는 무관하다.
    void ApplyGravity(Block* block);

    // block이 차지하는 칸들 중 바닥을 가장 깊이 파고든 정도를 찾아서, 파고들었으면 밀어냄
    // [좌우 편향 방지] swapContactOrder 설명은 ResolveBlockPairCollision 주석 참고.
    void ResolveFloorCollision(Block* block, bool swapContactOrder);

    // [책임 분리] "겹쳤는지/어디를/얼마나 겹쳤는지" 판정은 CollisionManager::DetectPairCollision이 전담한다.
    // 여기서는 그 결과를 가지고 "그래서 어떻게 반응할지"(WakeUp 여부, 임펄스 적용)만 담당한다.
    // 둘 다 Awake면 쌍방향(ResolveRigidCollisionWithBlock), 한쪽만 Awake면 Sleeping인 쪽을
    // 바닥처럼 고정된 것으로 취급(ResolveRigidCollision)한다.
    // [좌우 편향 방지 — 세로 I블럭이 완전 대칭으로 착지해도 매번 같은 방향(오른쪽)으로 도는 버그 수정]
    // CollisionManager::BuildManifold가 manifold의 두 접촉점을 항상 "왼쪽=[0], 오른쪽=[1]" 순서로
    // 결정론적으로 고정해서 돌려주는데, 이 둘을 순차(왼쪽 먼저 -> 오른쪽 나중)로 풀면 먼저 처리된 점이
    // 나중 점의 계산에 쓰일 속도/각속도를 먼저 바꿔버려서, 완벽하게 좌우 대칭인 충돌(예: 세로로 곧게
    // 선 I블럭이 수평으로 바닥에 닿는 경우)에서도 매번 같은 부호의 잔여 각속도가 남는다. 이 잔여값이
    // 작아도 매 프레임 항상 같은 방향이라 Integrate()에 누적되며 "무조건 한쪽으로 도는 힘"으로 보인다.
    // 두 점을 진짜 연립으로 동시에 푸는 것(예전에 임펄스 폭주 버그로 되돌렸던 방식)보다 안전한 최소
    // 수정으로, swapContactOrder는 어느 쪽이 먼저 처리되는지를 뒤집는 스위치다 — 실제로 어떤 주기로
    // 뒤집어야 편향이 상쇄되는지는 PhysicsManager.h의 m_swapContactOrderThisStep 선언부 주석 참고
    // (반복 단위로 뒤집으면 안 되고 프레임 단위로 뒤집어야 한다).
    void ResolveBlockPairCollision(Block* block, Block* other, bool swapContactOrder);
    // 존재하는 모든 블럭 쌍(중복 없이 한 번씩)에 대해 ResolveBlockPairCollision을 돌림
    void ResolveBlockCollisions(bool swapContactOrder);

    // [레이캐스트 기반] 이 칸의 바닥 모서리(왼쪽 끝/가운데/오른쪽 끝)에서 각각 수직 아래로 레이를 쏴서,
    // 그 지점 바로 아래에 실제로 지지면(바닥 또는 다른 블럭의 윗면)이 있는지 확인한다. 칸의 X범위가
    // 지지대의 X범위와 "겹치는지"만 보는 방식(GetCellSupportRange)과 달리, 서로 떨어진 두 지지대 사이의
    // 완전한 허공 위에 칸이 걸친 경우까지 세 지점 중 하나라도 실제로 뭔가에 맞았는지로 직접 확인한다.
    // Sleep 가능 여부/붕괴 판정처럼 "여기서 잠들어도/버텨도 되는가"를 결정하는 곳에서만 쓴다.
    bool IsCellSupported(Block* block, int cellIndex) const;

    // [경계값 버그 방지 — 실전 확인됨] IsCellSupported와 같은 기준으로 지지 여부를 판정하되, 지지된다면
    // 그 칸의 전체 폭이 아니라 "바닥/아래 블록과 실제로 겹치는 부분"만 outSupportMinX/MaxX로 돌려준다.
    // 칸이 발판이나 아래 블록의 가장자리에 절반쯤만 걸쳤을 때(칸 중심은 지지 쪽에 있지만 칸 자체는 절반이
    // 허공 위) 칸의 전체 폭을 지지 범위에 다 넣으면, 실제보다 지지 기반이 넓게(허공 쪽으로) 잡혀서 무게중심이
    // 그 안에 들어와버려 명백히 넘어져야 할 상황에서도 안 넘어지는 원인이 된다 — 4칸 중 3칸이 기둥으로
    // 곧게 서 있고 발만 옆으로 삐져나온 모양이 착지 위치에 따라 이렇게 잘못 안정 판정되는 걸 확인했다.
    // ComputeSupportDebugInfo(진짜 지지 범위/무게중심 비교가 필요한 곳)만 이 버전을 쓴다.
    bool GetCellSupportRange(Block* block, int cellIndex, float& outSupportMinX, float& outSupportMaxX) const;

    // [마찰 사각지대 방지] IsCellSupported보다 느슨하게, 칸이 바닥/다른 블럭과 조금이라도 겹치면 true를
    // 반환한다. 회전한 칸이 모서리로만 걸친 경우(IsCellSupported는 false) 이 함수는 true가 되는데, 이게
    // 필요한 이유: 마찰 감쇠(Step 2.5)를 IsCellSupported 기준으로만 걸면, "모서리로만 걸쳐서 중심은 안
    // 겹치는" 블록은 마찰도 안 받고 안정 판정도 못 받는 사각지대에 빠져 중력에 살짝 밀렸다가 충돌에 다시
    // 튕기는 게 감쇠 없이 반복되며 영원히 떨린다. 마찰은 이 느슨한 기준으로 걸어서 떨림 자체를 죽이고,
    // "진짜 안정적인지/넘어질지"는 여전히 IsCellSupported(엄격)로 따로 판단한다.
    bool IsCellTouchingAnySupport(Block* block, int cellIndex) const;
    // [무게중심 실제 토크] block의 무게중심(x)이 지지된 칸들의 가로 범위를 벗어났으면
    // Block::ApplyGravityTorque로 pivot(지지 가장자리) 기준 진짜 중력 토크를 매 프레임 걸어준다.
    // 기울어진 각도가 MAX_TOPPLE_ANGLE을 넘어서야 실제로 BeginToppling() 상태 전환이 일어나고,
    // 균형 잡힌 경우엔 아무것도 안 한다.
    // [성능] childrenOf는 이번 물리 스텝에서 BuildRestingChildrenMap()으로 한 번만 계산해서 매 블럭 호출에
    // 재사용하는 "누가 누구 위에 얹혀 있는지" 맵이다 — 매번 새로 스캔하지 않기 위한 것.
    // [지지대 우선 붕괴] 반환값은 "이번 프레임에 이 블록이 불균형으로 판정돼 처리(토크 적용/포기 등)됐는지"
    // — Step()이 true를 받으면 이 블록 위에 얹힌 모든 블록(재귀)을 이번 프레임엔 각자 따로 불균형 검사를
    // 안 하도록 건너뛴다. "지지대가 먼저 무너지는 게 맞다"는 설계 결정에 따른 것 — 아래(더 근본적인 지지대)가
    // 이미 불안정을 처리 중이면, 그 위에 얹힌 것들은 자기 좁은 접촉면만 보고 따로 넘어지지 않고 아래의
    // 붕괴에 묻어간다. 아래가 실제로 넘어지면(Toppling) 다음 프레임에 지지를 잃었다고 자연히 감지해서 반응한다.
    bool ResolveBalance(Block* block, float deltaTime, const RestingChildrenMap& childrenOf);

    // Toppling 상태인 블럭 중 화면 아래로 완전히 벗어난 것들을 BlockManager에서 제거
    void RemoveToppledBlocks();

    // Toppling 상태인 블럭 중 화면 밖으로 떨어지지 않고, 결합 무게중심이 실제 지지 범위 안으로
    // 들어온 것들만 Awake로 전환한다("칸 하나라도 닿음"만 보고 돌려보내면, 회전하며 넘어지는 도중
    // 다른 모서리가 잠깐 스치기만 해도 매번 Awake로 튕겨나갔다가 다음 스텝에 ResolveBalance가 다시
    // 넘어뜨리는 걸 반복해서 — 진짜 넘어지는 중인 블록의 낙하가 계속 끊기고, 결국 재넘어짐 제한에
    // 걸려 넘어져야 할 자세 그대로 얼어붙었다). childrenOf는 이 스텝에서 이미 계산된 것을 그대로 받아
    // 재사용한다(성능, ResolveBalance와 같은 이유). Sleeping으로 곧바로 재우진 않는다 — 그건 TrySleepAll의
    // rest timer가 판단한다.
    void SettleToppledBlocks(const RestingChildrenMap& childrenOf);

    // 개별 블럭이 아니라 탑 전체가 안정적인지 판단 (전체 판단은 매니저의 책임)
    bool CheckGlobalStability() const;

    void TrySleepAll(float deltaTime);
	

    // [디버그 시각화] block 자신의 지지 범위(outMinX/outMaxX)와, block이 떠받치는 전체 덩어리의 결합
    // 무게중심(outCombinedComX)을 계산해서 그대로 돌려준다 — ResolveBalance와 완전히 같은 계산이라,
    // F1 디버그 오버레이가 "지금 이 블럭이 안정적이라고 판단된 이유"를 화면에 그대로 그릴 수 있게 해준다.
    // 지지대가 없으면 false를 반환(이 경우 outMinX/outMaxX/outCombinedComX는 의미 없음)
    bool ComputeSupportDebugInfo(Block* block, const RestingChildrenMap& childrenOf, float& outMinX, float& outMaxX, float& outCombinedComX) const;

    // [성능] "누가 누구 위에 얹혀 있는지"를 전체 블럭 쌍을 훑어서 한 번에 계산해둔다. 물리 스텝(Step)뿐
    // 아니라 디버그 오버레이(RenderManager::DrawSupportDebug)도 이걸 한 번만 만들어서 재사용해야 한다.
    // [무게 분배] 자식 하나가 여러 base 위에 걸쳐 있으면, 그 자식이 각 base 위에 몇 칸씩 얹혀 있는지를
    // 세서 총 얹힌 칸 수 대비 비율로 무게를 나눠 배분한다 — 안 그러면 각 base가 "내가 그 자식 무게
    // 전체를 다 받친다"고 중복 계산해서, 실제로는 두 base가 나눠 받쳐 안정적인 구조도 양쪽 다 불안정으로
    // 오판한다(L자 블록이 서로 다른 두 블록 위에 한 칸씩 걸쳤을 때 둘 다 무너지던 버그로 실전 확인됨).
    RestingChildrenMap BuildRestingChildrenMap() const;

private:
    PhysicsManager() = default;
    ~PhysicsManager();
    PhysicsManager(const PhysicsManager&) = delete;
    PhysicsManager& operator=(const PhysicsManager&) = delete;
    // [지지 판정 통합] cellIndex번 칸의 회전된 네 꼭짓점(GetCellRotatedCorners)에서 "가장 아래 Y"와
    // "가로 범위(min~max X)"를 뽑아준다. 회전이 없으면 기존 cellBottomY/cellCenterX 계산과 똑같이 나오고,
    // 기울어진 블럭이면 실제 기운 모양대로 나온다 — IsCellSupported/CountCellsRestingOnBlock/
    // ComputeSupportDebugInfo가 전부 이 함수 하나로 지지 판정용 좌표를 얻게 해서, 세 곳이 서로 다른
    // 좌표계를 보는 문제를 없앤다.
    void GetCellSupportBounds(Block* block, int cellIndex, float& outTopY, float& outBottomY, float& outMinX, float& outMaxX) const;
    // upper의 칸 중 몇 개가 lower 위에 얹혀 있는지(IsCellSupported와 같은 판정을, 특정 블록 한 쌍으로
    // 좁혀서) 센다. BuildRestingChildrenMap이 이 개수로 무게 분배 비율을 계산하는 데 쓴다 — 예전엔
    // "하나라도 얹혔는지"만 boolean으로 봤는데, 그러면 여러 base에 걸친 블록의 무게를 base마다 몇 칸씩
    // 받치는지 구분 못 해서 나눠 계산할 수가 없었다.
    int CountCellsRestingOnBlock(Block* upper, Block* lower) const;

    // [레이캐스트] origin에서 수직 아래 방향으로 레이를 쏴서, X가 origin.x를 포함하는 지지면(바닥 또는
    // 다른 블럭의 윗면) 중 가장 가까운 것까지의 거리를 outHitDistance로 돌려준다(못 찾으면 false).
    // selfBlock은 레이를 쏘는 블럭 자신 — 자기 자신은 지지 제공자 후보에서 제외해야 해서 필요하다.
    // IsCellSupported가 이 함수로 칸의 바닥 모서리를 따라 여러 지점을 검사하는 데 쓴다.
    bool RaycastDownForSupport(Vector2 origin, Block* selfBlock, float& outHitDistance) const;

    // [고정 timestep] 예전 Update()의 본문 전체 — 항상 PHYSICS_FIXED_TIMESTEP 크기로만 호출된다.
    // Update()가 렌더 프레임의 가변 deltaTime을 여기로 몇 번 나눠서 넘길지 결정한다.
    void Step(float deltaTime);

    // [고정 timestep] 프레임 간 남은 시간을 누적해뒀다가, PHYSICS_FIXED_TIMESTEP만큼씩 Step()에 넘긴다.
    float m_accumulator = 0.0f;

    // [좌우 편향 방지 — 반복 단위 교대의 한계 보완] 반복(iteration) 안에서 순서를 뒤집으면, 뒤늦은
    // 반복일수록 처리할 잔여 속도가 이미 작아져 있어서 편향 크기가 훨씬 작다 — "왼쪽 먼저"인 앞쪽
    // 반복(0, 2번째)의 큰 편향이 "오른쪽 먼저"인 뒤쪽 반복(1, 3번째)의 작은 편향으로 다 상쇄되지
    // 못하고 매 프레임 같은 방향(왼쪽 먼저 쪽)으로 순 잔차가 남는다. 그 대신 이 프레임 전체(4회 반복
    // 전부)를 한 방향으로 통일하고, 그 방향을 프레임마다(Step() 호출마다) 뒤집는다 — 그러면 크기가
    // 비슷한 "이번 프레임 전체 편향"과 "다음 프레임 전체 편향"이 반대 부호로 맞물려서 훨씬 깨끗하게 상쇄된다.
    bool m_swapContactOrderThisStep = false;

    // [붕괴 충돌음] 이번 물리 스텝(Step)에서 이미 붕괴 충돌음을 재생했는지. ResolveBlockCollisions가
    // 한 스텝 안에서 4번(COLLISION_SOLVER_ITERATIONS) 반복 호출되고 부딪히는 쌍도 여러 개일 수 있어서,
    // 이 플래그 없이 조건이 맞을 때마다 바로 재생하면 붕괴 한 번에 소리가 수십 번 겹쳐 울린다.
    // Step() 맨 앞에서 매번 false로 리셋된다.
    bool m_collisionSfxPlayedThisStep = false;

    // [연쇄 붕괴] base 위에 (직접 또는 다른 블럭을 거쳐 간접적으로) 얹힌 모든 블럭을 재귀로 훑어서,
    // base를 포함한 전체의 질량 합과 무게중심(x) 가중합을 누적한다. ResolveBalance가 "이 블럭 자신"이 아니라
    // "이 블럭이 떠받치고 있는 전체 무더기"의 무게중심으로 판정하게 하려고 필요하다 —
    // 안 그러면 위에 삐딱하게 얹힌 블럭만 넘어지고 밑을 받치는 블럭은 자기 발판만 보고 멀쩡하다고 착각한다.
    // [무게 분배] weightFraction은 지금까지 내려오면서 곱해진 "이 base가 자신의 위쪽 경로를 통해 실제로
    // 부담하는 비율"(0~1)이다. 최상위 호출은 1.0(자기 자신은 전부 자기 몫)로 시작하고, 자식으로 재귀할
    // 때마다 그 자식이 여러 base에 걸쳐 있으면 childrenOf에 저장된 자식별 비율을 곱해서 내려간다 —
    // 그래야 여러 base에 나눠 걸친 자식의 무게가 각 base에 중복 없이 비율만큼만 반영된다.
    void AccumulateSupportedMass(Block* base, float weightFraction, std::vector<Block*>& visited, const RestingChildrenMap& childrenOf, float& outTotalMass, float& outWeightedX) const;
};
